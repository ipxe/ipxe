/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <byteswap.h>
#include <gpxe/if_ether.h>
#include <gpxe/netdevice.h>
#include <gpxe/device.h>
#include <gpxe/xfer.h>
#include <gpxe/open.h>
#include <gpxe/job.h>
#include <gpxe/retry.h>
#include <gpxe/tcpip.h>
#include <gpxe/ip.h>
#include <gpxe/uuid.h>
#include <gpxe/dhcp.h>
#include <gpxe/timer.h>
#include <gpxe/settings.h>
#include <gpxe/dhcp.h>
#include <gpxe/dhcpopts.h>
#include <gpxe/dhcppkt.h>

/** @file
 *
 * Dynamic Host Configuration Protocol
 *
 */

/**
 * DHCP operation types
 *
 * This table maps from DHCP message types (i.e. values of the @c
 * DHCP_MESSAGE_TYPE option) to values of the "op" field within a DHCP
 * packet.
 */
static const uint8_t dhcp_op[] = {
	[DHCPDISCOVER]	= BOOTP_REQUEST,
	[DHCPOFFER]	= BOOTP_REPLY,
	[DHCPREQUEST]	= BOOTP_REQUEST,
	[DHCPDECLINE]	= BOOTP_REQUEST,
	[DHCPACK]	= BOOTP_REPLY,
	[DHCPNAK]	= BOOTP_REPLY,
	[DHCPRELEASE]	= BOOTP_REQUEST,
	[DHCPINFORM]	= BOOTP_REQUEST,
};

/** Raw option data for options common to all DHCP requests */
static uint8_t dhcp_request_options_data[] = {
	DHCP_MAX_MESSAGE_SIZE,
	DHCP_WORD ( ETH_MAX_MTU - 20 /* IP header */ - 8 /* UDP header */ ),
	DHCP_CLIENT_ARCHITECTURE, DHCP_WORD ( 0 ),
	DHCP_CLIENT_NDI, DHCP_OPTION ( 1 /* UNDI */ , 2, 1 /* v2.1 */ ),
	DHCP_VENDOR_CLASS_ID,
	DHCP_STRING (  'P', 'X', 'E', 'C', 'l', 'i', 'e', 'n', 't', ':',
		       'A', 'r', 'c', 'h', ':', '0', '0', '0', '0', '0', ':',
		       'U', 'N', 'D', 'I', ':', '0', '0', '2', '0', '0', '1' ),
	DHCP_PARAMETER_REQUEST_LIST,
	DHCP_OPTION ( DHCP_SUBNET_MASK, DHCP_ROUTERS, DHCP_DNS_SERVERS,
		      DHCP_LOG_SERVERS, DHCP_HOST_NAME, DHCP_DOMAIN_NAME,
		      DHCP_ROOT_PATH, DHCP_VENDOR_ENCAP, DHCP_VENDOR_CLASS_ID,
		      DHCP_TFTP_SERVER_NAME, DHCP_BOOTFILE_NAME,
		      DHCP_EB_ENCAP, DHCP_ISCSI_INITIATOR_IQN ),
	DHCP_END
};

/** Options common to all DHCP requests */
static struct dhcp_options dhcp_request_options = {
	.data = dhcp_request_options_data,
	.max_len = sizeof ( dhcp_request_options_data ),
	.len = sizeof ( dhcp_request_options_data ),
};

/** DHCP feature codes */
static uint8_t dhcp_features[0] __table_start ( uint8_t, dhcp_features );
static uint8_t dhcp_features_end[0] __table_end ( uint8_t, dhcp_features );

/** DHCP network device descriptor */
struct dhcp_netdev_desc {
	/** Bus type ID */
	uint8_t type;
	/** Vendor ID */
	uint16_t vendor;
	/** Device ID */
	uint16_t device;
} __attribute__ (( packed ));

/** DHCP client identifier */
struct dhcp_client_id {
	/** Link-layer protocol */
	uint8_t ll_proto;
	/** Link-layer address */
	uint8_t ll_addr[MAX_LL_ADDR_LEN];
} __attribute__ (( packed ));

/** DHCP client UUID */
struct dhcp_client_uuid {
	/** Identifier type */
	uint8_t type;
	/** UUID */
	union uuid uuid;
} __attribute__ (( packed ));

#define DHCP_CLIENT_UUID_TYPE 0

/**
 * Name a DHCP packet type
 *
 * @v msgtype		DHCP message type
 * @ret string		DHCP mesasge type name
 */
static inline const char * dhcp_msgtype_name ( unsigned int msgtype ) {
	switch ( msgtype ) {
	case DHCPNONE:		return "BOOTP"; /* Non-DHCP packet */
	case DHCPDISCOVER:	return "DHCPDISCOVER";
	case DHCPOFFER:		return "DHCPOFFER";
	case DHCPREQUEST:	return "DHCPREQUEST";
	case DHCPDECLINE:	return "DHCPDECLINE";
	case DHCPACK:		return "DHCPACK";
	case DHCPNAK:		return "DHCPNAK";
	case DHCPRELEASE:	return "DHCPRELEASE";
	case DHCPINFORM:	return "DHCPINFORM";
	default:		return "DHCP<invalid>";
	}
}

/**
 * Calculate DHCP transaction ID for a network device
 *
 * @v netdev		Network device
 * @ret xid		DHCP XID
 *
 * Extract the least significant bits of the hardware address for use
 * as the transaction ID.
 */
static uint32_t dhcp_xid ( struct net_device *netdev ) {
	uint32_t xid;

	memcpy ( &xid, ( netdev->ll_addr + netdev->ll_protocol->ll_addr_len
			 - sizeof ( xid ) ), sizeof ( xid ) );
	return xid;
}

/****************************************************************************
 *
 * DHCP settings
 *
 */

/** A DHCP settings block */
struct dhcp_settings {
	/** Reference counter */
	struct refcnt refcnt;
	/** DHCP packet */
	struct dhcp_packet dhcppkt;
	/** Setting interface */
	struct settings settings;
};

/**
 * Increment reference count on DHCP settings block
 *
 * @v dhcpset		DHCP settings block
 * @ret dhcpset		DHCP settings block
 */
static inline __attribute__ (( always_inline )) struct dhcp_settings *
dhcpset_get ( struct dhcp_settings *dhcpset ) {
	ref_get ( &dhcpset->refcnt );
	return dhcpset;
}

/**
 * Decrement reference count on DHCP settings block
 *
 * @v dhcpset		DHCP settings block
 */
static inline __attribute__ (( always_inline )) void
dhcpset_put ( struct dhcp_settings *dhcpset ) {
	ref_put ( &dhcpset->refcnt );
}

/**
 * Store value of DHCP setting
 *
 * @v settings		Settings block
 * @v setting		Setting to store
 * @v data		Setting data, or NULL to clear setting
 * @v len		Length of setting data
 * @ret rc		Return status code
 */
static int dhcpset_store ( struct settings *settings, struct setting *setting,
			   const void *data, size_t len ) {
	struct dhcp_settings *dhcpset =
		container_of ( settings, struct dhcp_settings, settings );

	return dhcppkt_store ( &dhcpset->dhcppkt, setting->tag, data, len );
}

/**
 * Fetch value of DHCP setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int dhcpset_fetch ( struct settings *settings, struct setting *setting,
			   void *data, size_t len ) {
	struct dhcp_settings *dhcpset =
		container_of ( settings, struct dhcp_settings, settings );

	return dhcppkt_fetch ( &dhcpset->dhcppkt, setting->tag, data, len );
}

/** DHCP settings operations */
static struct settings_operations dhcpset_settings_operations = {
	.store = dhcpset_store,
	.fetch = dhcpset_fetch,
};

/**
 * Create DHCP setting block
 *
 * @v dhcphdr		DHCP packet
 * @v len		Length of DHCP packet
 * @ret dhcpset		DHCP settings block
 */
static struct dhcp_settings * dhcpset_create ( const struct dhcphdr *dhcphdr,
					       size_t len ) {
	struct dhcp_settings *dhcpset;
	void *data;

	dhcpset = zalloc ( sizeof ( *dhcpset ) + len );
	if ( dhcpset ) {
		data = ( ( ( void * ) dhcpset ) + sizeof ( *dhcpset ) );
		memcpy ( data, dhcphdr, len );
		dhcppkt_init ( &dhcpset->dhcppkt, data, len );
		settings_init ( &dhcpset->settings,
				&dhcpset_settings_operations, &dhcpset->refcnt,
				DHCP_SETTINGS_NAME, 0 );
	}
	return dhcpset;
}

/** DHCP server address setting */
struct setting dhcp_server_setting __setting = {
	.name = "dhcp-server",
	.description = "DHCP server address",
	.tag = DHCP_SERVER_IDENTIFIER,
	.type = &setting_type_ipv4,
};

/****************************************************************************
 *
 * DHCP session
 *
 */

/** DHCP session states */
enum dhcp_session_state {
	/** Sending DHCPDISCOVERs, collecting DHCPOFFERs and ProxyDHCPOFFERs */
	DHCP_STATE_DISCOVER = 0,
	/** Sending DHCPREQUESTs, waiting for DHCPACK */
	DHCP_STATE_REQUEST,
	/** Sending ProxyDHCPREQUESTs, waiting for ProxyDHCPACK */
	DHCP_STATE_PROXYREQUEST,
};

/**
 * Name a DHCP session state
 *
 * @v state		DHCP session state
 * @ret string		DHCP session state name
 */
static inline const char * dhcp_state_name ( enum dhcp_session_state state ) {
	switch ( state ) {
	case DHCP_STATE_DISCOVER:	return "DHCPDISCOVER";
	case DHCP_STATE_REQUEST:	return "DHCPREQUEST";
	case DHCP_STATE_PROXYREQUEST:	return "ProxyDHCPREQUEST";
	default:			return "<invalid>";
	}
}

/** A DHCP session */
struct dhcp_session {
	/** Reference counter */
	struct refcnt refcnt;
	/** Job control interface */
	struct job_interface job;
	/** Data transfer interface */
	struct xfer_interface xfer;

	/** Network device being configured */
	struct net_device *netdev;

	/** State of the session
	 *
	 * This is a value for the @c DHCP_MESSAGE_TYPE option
	 * (e.g. @c DHCPDISCOVER).
	 */
	enum dhcp_session_state state;
	/** DHCPOFFER obtained during DHCPDISCOVER */
	struct dhcp_settings *dhcpoffer;
	/** ProxyDHCPOFFER obtained during DHCPDISCOVER */
	struct dhcp_settings *proxydhcpoffer;
	/** Retransmission timer */
	struct retry_timer timer;
	/** Start time of the current state (in ticks) */
	unsigned long start;
};

/**
 * Free DHCP session
 *
 * @v refcnt		Reference counter
 */
static void dhcp_free ( struct refcnt *refcnt ) {
	struct dhcp_session *dhcp =
		container_of ( refcnt, struct dhcp_session, refcnt );

	netdev_put ( dhcp->netdev );
	dhcpset_put ( dhcp->dhcpoffer );
	dhcpset_put ( dhcp->proxydhcpoffer );
	free ( dhcp );
}

/**
 * Mark DHCP session as complete
 *
 * @v dhcp		DHCP session
 * @v rc		Return status code
 */
static void dhcp_finished ( struct dhcp_session *dhcp, int rc ) {

	/* Block futher incoming messages */
	job_nullify ( &dhcp->job );
	xfer_nullify ( &dhcp->xfer );

	/* Stop retry timer */
	stop_timer ( &dhcp->timer );

	/* Free resources and close interfaces */
	xfer_close ( &dhcp->xfer, rc );
	job_done ( &dhcp->job, rc );
}

/****************************************************************************
 *
 * Data transfer interface
 *
 */

/**
 * Create a DHCP packet
 *
 * @v dhcppkt		DHCP packet structure to fill in
 * @v netdev		Network device
 * @v msgtype		DHCP message type
 * @v options		Initial options to include (or NULL)
 * @v data		Buffer for DHCP packet
 * @v max_len		Size of DHCP packet buffer
 * @ret rc		Return status code
 *
 * Creates a DHCP packet in the specified buffer, and fills out a @c
 * dhcp_packet structure.
 */
int dhcp_create_packet ( struct dhcp_packet *dhcppkt,
			 struct net_device *netdev, uint8_t msgtype,
			 struct dhcp_options *options, 
			 void *data, size_t max_len ) {
	struct dhcphdr *dhcphdr = data;
	size_t options_len;
	unsigned int hlen;
	int rc;

	/* Sanity check */
	options_len = ( options ? options->len : 0 );
	if ( max_len < ( sizeof ( *dhcphdr ) + options_len ) )
		return -ENOSPC;

	/* Initialise DHCP packet content */
	memset ( dhcphdr, 0, max_len );
	dhcphdr->xid = dhcp_xid ( netdev );
	dhcphdr->magic = htonl ( DHCP_MAGIC_COOKIE );
	dhcphdr->htype = ntohs ( netdev->ll_protocol->ll_proto );
	dhcphdr->op = dhcp_op[msgtype];
	/* If hardware length exceeds the chaddr field length, don't
	 * use the chaddr field.  This is as per RFC4390.
	 */
	hlen = netdev->ll_protocol->ll_addr_len;
	if ( hlen > sizeof ( dhcphdr->chaddr ) ) {
		hlen = 0;
		dhcphdr->flags = htons ( BOOTP_FL_BROADCAST );
	}
	dhcphdr->hlen = hlen;
	memcpy ( dhcphdr->chaddr, netdev->ll_addr, hlen );
	memcpy ( dhcphdr->options, options->data, options_len );

	/* Initialise DHCP packet structure */
	memset ( dhcppkt, 0, sizeof ( *dhcppkt ) );
	dhcppkt_init ( dhcppkt, data, max_len );
	
	/* Set DHCP_MESSAGE_TYPE option */
	if ( ( rc = dhcppkt_store ( dhcppkt, DHCP_MESSAGE_TYPE,
				    &msgtype, sizeof ( msgtype ) ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Create DHCP request packet
 *
 * @v dhcppkt		DHCP packet structure to fill in
 * @v netdev		Network device
 * @v ciaddr		Client IP address
 * @v offer		DHCP offer, if applicable
 * @v data		Buffer for DHCP packet
 * @v max_len		Size of DHCP packet buffer
 * @ret rc		Return status code
 */
int dhcp_create_request ( struct dhcp_packet *dhcppkt,
			  struct net_device *netdev, struct in_addr ciaddr,
			  struct dhcp_packet *offer,
			  void *data, size_t max_len ) {
	struct device_description *desc = &netdev->dev->desc;
	struct dhcp_netdev_desc dhcp_desc;
	struct dhcp_client_id client_id;
	struct dhcp_client_uuid client_uuid;
	unsigned int msgtype;
	size_t dhcp_features_len;
	size_t ll_addr_len;
	int rc;

	/* Create DHCP packet */
	msgtype = ( offer ? DHCPREQUEST : DHCPDISCOVER );
	if ( ( rc = dhcp_create_packet ( dhcppkt, netdev, msgtype,
					 &dhcp_request_options, data,
					 max_len ) ) != 0 ) {
		DBG ( "DHCP could not create DHCP packet: %s\n",
		      strerror ( rc ) );
		return rc;
	}

	/* Set client IP address */
	dhcppkt->dhcphdr->ciaddr = ciaddr;

	/* Copy any required options from previous server repsonse */
	if ( offer ) {
		struct in_addr server = { 0 };
		struct in_addr *ip = &offer->dhcphdr->yiaddr;

		/* Copy server identifier, if present */
		if ( ( dhcppkt_fetch ( offer, DHCP_SERVER_IDENTIFIER, &server,
				       sizeof ( server ) ) >= 0 ) &&
		     ( ( rc = dhcppkt_store ( dhcppkt, DHCP_SERVER_IDENTIFIER,
					      &server,
					      sizeof ( server ) ) ) != 0 ) ) {
			DBG ( "DHCP could not set server ID: %s\n",
			      strerror ( rc ) );
			return rc;
		}

		/* Copy requested IP address, if present */
		if ( ( ip->s_addr != 0 ) &&
		     ( ( rc = dhcppkt_store ( dhcppkt, DHCP_REQUESTED_ADDRESS,
					      ip, sizeof ( *ip ) ) ) != 0 ) ) {
			DBG ( "DHCP could not set requested address: %s\n",
			      strerror ( rc ) );
			return rc;
		}
	}

	/* Add options to identify the feature list */
	dhcp_features_len = ( dhcp_features_end - dhcp_features );
	if ( ( rc = dhcppkt_store ( dhcppkt, DHCP_EB_ENCAP, dhcp_features,
				    dhcp_features_len ) ) != 0 ) {
		DBG ( "DHCP could not set features list option: %s\n",
		      strerror ( rc ) );
		return rc;
	}

	/* Add options to identify the network device */
	dhcp_desc.type = desc->bus_type;
	dhcp_desc.vendor = htons ( desc->vendor );
	dhcp_desc.device = htons ( desc->device );
	if ( ( rc = dhcppkt_store ( dhcppkt, DHCP_EB_BUS_ID, &dhcp_desc,
				    sizeof ( dhcp_desc ) ) ) != 0 ) {
		DBG ( "DHCP could not set bus ID option: %s\n",
		      strerror ( rc ) );
		return rc;
	}

	/* Add DHCP client identifier.  Required for Infiniband, and
	 * doesn't hurt other link layers.
	 */
	client_id.ll_proto = ntohs ( netdev->ll_protocol->ll_proto );
	ll_addr_len = netdev->ll_protocol->ll_addr_len;
	assert ( ll_addr_len <= sizeof ( client_id.ll_addr ) );
	memcpy ( client_id.ll_addr, netdev->ll_addr, ll_addr_len );
	if ( ( rc = dhcppkt_store ( dhcppkt, DHCP_CLIENT_ID, &client_id,
				    ( ll_addr_len + 1 ) ) ) != 0 ) {
		DBG ( "DHCP could not set client ID: %s\n",
		      strerror ( rc ) );
		return rc;
	}

	/* Add client UUID, if we have one.  Required for PXE. */
	client_uuid.type = DHCP_CLIENT_UUID_TYPE;
	if ( ( rc = fetch_uuid_setting ( NULL, &uuid_setting,
					 &client_uuid.uuid ) ) >= 0 ) {
		if ( ( rc = dhcppkt_store ( dhcppkt, DHCP_CLIENT_UUID,
					    &client_uuid,
					    sizeof ( client_uuid ) ) ) != 0 ) {
			DBG ( "DHCP could not set client UUID: %s\n",
			      strerror ( rc ) );
			return rc;
		}
	}

	return 0;
}

/**
 * Transmit DHCP request
 *
 * @v dhcp		DHCP session
 * @ret rc		Return status code
 */
static int dhcp_tx ( struct dhcp_session *dhcp ) {
	static struct sockaddr_in proxydhcp_server = {
		.sin_family = AF_INET,
		.sin_port = htons ( PROXYDHCP_PORT ),
	};
	struct xfer_metadata meta = {
		.netdev = dhcp->netdev,
	};
	struct io_buffer *iobuf;
	struct dhcp_packet dhcppkt;
	struct dhcp_packet *offer = NULL;
	struct in_addr ciaddr = { 0 };
	int check_len;
	int rc;

	/* Start retry timer.  Do this first so that failures to
	 * transmit will be retried.
	 */
	start_timer ( &dhcp->timer );

	/* Determine packet contents based on current state */
	switch ( dhcp->state ) {
	case DHCP_STATE_DISCOVER:
		DBGC ( dhcp, "DHCP %p transmitting DHCPDISCOVER\n", dhcp );
		break;
	case DHCP_STATE_REQUEST:
		DBGC ( dhcp, "DHCP %p transmitting DHCPREQUEST\n", dhcp );
		assert ( dhcp->dhcpoffer );
		offer = &dhcp->dhcpoffer->dhcppkt;
		break;
	case DHCP_STATE_PROXYREQUEST:
		DBGC ( dhcp, "DHCP %p transmitting ProxyDHCPREQUEST\n", dhcp );
		assert ( dhcp->dhcpoffer );
		assert ( dhcp->proxydhcpoffer );
		offer = &dhcp->proxydhcpoffer->dhcppkt;
		ciaddr = dhcp->dhcpoffer->dhcppkt.dhcphdr->yiaddr;
		check_len = dhcppkt_fetch ( offer, DHCP_SERVER_IDENTIFIER,
					    &proxydhcp_server.sin_addr,
					    sizeof(proxydhcp_server.sin_addr));
		meta.dest = ( struct sockaddr * ) &proxydhcp_server;
		assert ( ciaddr.s_addr != 0 );
		assert ( proxydhcp_server.sin_addr.s_addr != 0 );
		assert ( check_len == sizeof ( proxydhcp_server.sin_addr ) );
		break;
	default:
		assert ( 0 );
		break;
	}

	/* Allocate buffer for packet */
	iobuf = xfer_alloc_iob ( &dhcp->xfer, DHCP_MIN_LEN );
	if ( ! iobuf )
		return -ENOMEM;

	/* Create DHCP packet in temporary buffer */
	if ( ( rc = dhcp_create_request ( &dhcppkt, dhcp->netdev,
					  ciaddr, offer, iobuf->data,
					  iob_tailroom ( iobuf ) ) ) != 0 ) {
		DBGC ( dhcp, "DHCP %p could not construct DHCP request: %s\n",
		       dhcp, strerror ( rc ) );
		goto done;
	}

	/* Transmit the packet */
	iob_put ( iobuf, dhcppkt.len );
	rc = xfer_deliver_iob_meta ( &dhcp->xfer, iobuf, &meta );
	iobuf = NULL;
	if ( rc != 0 ) {
		DBGC ( dhcp, "DHCP %p could not transmit UDP packet: %s\n",
		       dhcp, strerror ( rc ) );
		goto done;
	}

 done:
	free_iob ( iobuf );
	return rc;
}

/**
 * Transition to new DHCP session state
 *
 * @v dhcp		DHCP session
 * @v state		New session state
 */
static void dhcp_set_state ( struct dhcp_session *dhcp,
			     enum dhcp_session_state state ) {
	DBGC ( dhcp, "DHCP %p entering %s state\n",
	       dhcp, dhcp_state_name ( state ) );
	dhcp->state = state;
	dhcp->start = currticks();
	start_timer_nodelay ( &dhcp->timer );
}

/**
 * Store received DHCPOFFER
 *
 * @v dhcp		DHCP session
 * @v dhcpoffer		Received DHCPOFFER
 * @v stored_dhcpoffer	Location to store DHCPOFFER
 *
 * The DHCPOFFER will be stored in place of the existing stored
 * DHCPOFFER if its priority is equal to or greater than the stored
 * DHCPOFFER.
 */
static void dhcp_store_dhcpoffer ( struct dhcp_session *dhcp,
				   struct dhcp_settings *dhcpoffer,
				   struct dhcp_settings **stored_dhcpoffer ) {
	uint8_t stored_priority = 0;
	uint8_t priority = 0;

	/* Get priorities of the two DHCPOFFERs */
	if ( *stored_dhcpoffer ) {
		dhcppkt_fetch ( &(*stored_dhcpoffer)->dhcppkt,
				DHCP_EB_PRIORITY, &stored_priority,
				sizeof ( stored_priority ) );
	}
	dhcppkt_fetch ( &dhcpoffer->dhcppkt, DHCP_EB_PRIORITY, &priority,
			sizeof ( priority ) );

	/* Replace stored offer only if priority is equal or greater */
	if ( priority >= stored_priority ) {
		if ( *stored_dhcpoffer ) {
			DBGC ( dhcp, "DHCP %p stored DHCPOFFER %p discarded\n",
			       dhcp, *stored_dhcpoffer );
		}
		DBGC ( dhcp, "DHCP %p received DHCPOFFER %p stored\n",
		       dhcp, dhcpoffer );
		dhcpset_put ( *stored_dhcpoffer );
		*stored_dhcpoffer = dhcpset_get ( dhcpoffer );
	}
}

/**
 * Handle received DHCPOFFER
 *
 * @v dhcp		DHCP session
 * @v dhcpoffer		Received DHCPOFFER
 */
static void dhcp_rx_dhcpoffer ( struct dhcp_session *dhcp,
				struct dhcp_settings *dhcpoffer ) {
	struct in_addr server_id = { 0 };
	char vci[9]; /* "PXEClient" */
	int len;
	uint8_t ignore_proxy = 0;
	unsigned long elapsed;

	/* Check for presence of DHCP server ID */
	if ( dhcppkt_fetch ( &dhcpoffer->dhcppkt, DHCP_SERVER_IDENTIFIER,
			     &server_id, sizeof ( server_id ) )
	     != sizeof ( server_id ) ) {
		DBGC ( dhcp, "DHCP %p received DHCPOFFER %p missing server "
		       "identifier\n", dhcp, dhcpoffer );
		/* Could be a valid BOOTP offer; do not abort processing */
	}

	/* If there is an IP address, it's a normal DHCPOFFER */
	if ( dhcpoffer->dhcppkt.dhcphdr->yiaddr.s_addr != 0 ) {
		DBGC ( dhcp, "DHCP %p received DHCPOFFER %p from %s has IP "
		       "address\n",
		       dhcp, dhcpoffer, inet_ntoa ( server_id ) );
		dhcp_store_dhcpoffer ( dhcp, dhcpoffer, &dhcp->dhcpoffer );
	}

	/* If there is a "PXEClient" vendor class ID, it's a
	 * ProxyDHCPOFFER.  Note that it could be both a normal
	 * DHCPOFFER and a ProxyDHCPOFFER.
	 */
	len = dhcppkt_fetch ( &dhcpoffer->dhcppkt, DHCP_VENDOR_CLASS_ID,
			      vci, sizeof ( vci ) );
	if ( ( server_id.s_addr != 0 ) &&
	     ( len >= ( int ) sizeof ( vci ) ) &&
	     ( strncmp ( "PXEClient", vci, sizeof ( vci ) ) == 0 ) ) {
		DBGC ( dhcp, "DHCP %p received DHCPOFFER %p from %s is a "
		       "ProxyDHCPOFFER\n",
		       dhcp, dhcpoffer, inet_ntoa ( server_id ) );
		dhcp_store_dhcpoffer ( dhcp, dhcpoffer,
				       &dhcp->proxydhcpoffer );
	}

	/* We can transition to making the DHCPREQUEST when we have a
	 * valid DHCPOFFER, and either:
	 *
	 *  o  The DHCPOFFER instructs us to not wait for ProxyDHCP, or
	 *  o  We have a valid ProxyDHCPOFFER, or
         *  o  We have allowed sufficient time for ProxyDHCPOFFERs.
	 */

	/* If we don't yet have a DHCPOFFER, do nothing */
	if ( ! dhcp->dhcpoffer )
		return;

	/* If the DHCPOFFER instructs us to ignore ProxyDHCP, discard
	 * any ProxyDHCPOFFER
	 */
	dhcppkt_fetch ( &dhcp->dhcpoffer->dhcppkt, DHCP_EB_NO_PROXYDHCP,
			&ignore_proxy, sizeof ( ignore_proxy ) );
	if ( ignore_proxy && dhcp->proxydhcpoffer ) {
		DBGC ( dhcp, "DHCP %p discarding ProxyDHCPOFFER\n", dhcp );
		dhcpset_put ( dhcp->proxydhcpoffer );
		dhcp->proxydhcpoffer = NULL;
	}

	/* If we can't yet transition to DHCPREQUEST, do nothing */
	elapsed = ( currticks() - dhcp->start );
	if ( ! ( ignore_proxy || dhcp->proxydhcpoffer ||
		 ( elapsed > PROXYDHCP_WAIT_TIME ) ) )
		return;

	/* Transition to DHCPREQUEST */
	dhcp_set_state ( dhcp, DHCP_STATE_REQUEST );
}

/**
 * Store received DHCPACK
 *
 * @v dhcp		DHCP session
 * @v dhcpack		Received DHCPACK
 *
 * The DHCPACK will be registered as a settings block.
 */
static int dhcp_store_dhcpack ( struct dhcp_session *dhcp,
				struct dhcp_settings *dhcpack,
				struct settings *parent ) {
	struct settings *settings = &dhcpack->settings;
	struct settings *old_settings;
	int rc;

	/* Unregister any old settings obtained via DHCP */
	if ( ( old_settings = find_child_settings ( parent, settings->name ) ))
		unregister_settings ( old_settings );

	/* Register new settings */
	if ( ( rc = register_settings ( settings, parent ) ) != 0 ) {
		DBGC ( dhcp, "DHCP %p could not register settings: %s\n",
		       dhcp, strerror ( rc ) );
		dhcp_finished ( dhcp, rc ); /* This is a fatal error */
		return rc;
	}

	return 0;
}

/**
 * Handle received DHCPACK
 *
 * @v dhcp		DHCP session
 * @v dhcpack		Received DHCPACK
 */
static void dhcp_rx_dhcpack ( struct dhcp_session *dhcp,
			      struct dhcp_settings *dhcpack ) {
	struct settings *parent;
	struct in_addr offer_server_id = { 0 };
	struct in_addr ack_server_id = { 0 };
	int rc;

	/* Verify server ID matches */
	assert ( dhcp->dhcpoffer != NULL );
	dhcppkt_fetch ( &dhcp->dhcpoffer->dhcppkt, DHCP_SERVER_IDENTIFIER,
			&offer_server_id, sizeof ( offer_server_id ) );
	dhcppkt_fetch ( &dhcpack->dhcppkt, DHCP_SERVER_IDENTIFIER,
			&ack_server_id, sizeof ( ack_server_id ) );
	if ( offer_server_id.s_addr != ack_server_id.s_addr ) {
		DBGC ( dhcp, "DHCP %p ignoring DHCPACK with wrong server ID "
		       "%s\n", dhcp, inet_ntoa ( ack_server_id ) );
		return;
	}

	/* Register settings */
	parent = netdev_settings ( dhcp->netdev );
	if ( ( rc = dhcp_store_dhcpack ( dhcp, dhcpack, parent ) ) !=0 )
		return;

	/* If we have a ProxyDHCPOFFER, transition to PROXYDHCPREQUEST */
	if ( dhcp->proxydhcpoffer ) {
		dhcp->timer.min_timeout = 0;
		dhcp_set_state ( dhcp, DHCP_STATE_PROXYREQUEST );
		return;
	}

	/* Terminate DHCP */
	dhcp_finished ( dhcp, 0 );
}

/**
 * Handle received ProxyDHCPACK
 *
 * @v dhcp		DHCP session
 * @v proxydhcpack	Received ProxyDHCPACK
 */
static void dhcp_rx_proxydhcpack ( struct dhcp_session *dhcp,
				   struct dhcp_settings *proxydhcpack ) {
	struct in_addr offer_server_id = { 0 };
	struct in_addr ack_server_id = { 0 };
	int rc;

	/* Verify server ID matches */
	assert ( dhcp->proxydhcpoffer != NULL );
	dhcppkt_fetch ( &dhcp->proxydhcpoffer->dhcppkt, DHCP_SERVER_IDENTIFIER,
			&offer_server_id, sizeof ( offer_server_id ) );
	dhcppkt_fetch ( &proxydhcpack->dhcppkt, DHCP_SERVER_IDENTIFIER,
			&ack_server_id, sizeof ( ack_server_id ) );
	if ( offer_server_id.s_addr != ack_server_id.s_addr ) {
		DBGC ( dhcp, "DHCP %p ignoring ProxyDHCPACK with wrong server "
		       "ID %s\n", dhcp, inet_ntoa ( ack_server_id ) );
		return;
	}

	/* Rename settings */
	proxydhcpack->settings.name = PROXYDHCP_SETTINGS_NAME;

	/* Register settings */
	if ( ( rc = dhcp_store_dhcpack ( dhcp, proxydhcpack, NULL ) ) != 0 )
		return;

	/* Terminate DHCP */
	dhcp_finished ( dhcp, 0 );
}

/**
 * Receive new data
 *
 * @v xfer 		Data transfer interface
 * @v iobuf		I/O buffer
 * @v meta		Transfer metadata
 * @ret rc		Return status code
 */
static int dhcp_deliver_iob ( struct xfer_interface *xfer,
			      struct io_buffer *iobuf,
			      struct xfer_metadata *meta ) {
	struct dhcp_session *dhcp =
		container_of ( xfer, struct dhcp_session, xfer );
	struct sockaddr_tcpip *st_src;
	unsigned int src_port;
	struct dhcp_settings *dhcpset;
	struct dhcphdr *dhcphdr;
	uint8_t msgtype = 0;
	int rc = 0;

	/* Sanity checks */
	if ( ! meta ) {
		DBGC ( dhcp, "DHCP %p received packet without metadata\n",
		       dhcp );
		rc = -EINVAL;
		goto err_no_meta;
	}
	if ( ! meta->src ) {
		DBGC ( dhcp, "DHCP %p received packet without source port\n",
		       dhcp );
		rc = -EINVAL;
		goto err_no_src;
	}
	st_src = ( struct sockaddr_tcpip * ) meta->src;
	src_port = st_src->st_port;

	/* Convert packet into a DHCP settings block */
	dhcpset = dhcpset_create ( iobuf->data, iob_len ( iobuf ) );
	if ( ! dhcpset ) {
		DBGC ( dhcp, "DHCP %p could not store DHCP packet\n", dhcp );
		rc = -ENOMEM;
		goto err_dhcpset_create;
	}
	dhcphdr = dhcpset->dhcppkt.dhcphdr;

	/* Identify message type */
	dhcppkt_fetch ( &dhcpset->dhcppkt, DHCP_MESSAGE_TYPE, &msgtype,
			sizeof ( msgtype ) );
	DBGC ( dhcp, "DHCP %p received %s %p from port %d\n", dhcp,
	       dhcp_msgtype_name ( msgtype ), dhcpset, ntohs ( src_port ) );

	/* Check for matching transaction ID */
	if ( dhcphdr->xid != dhcp_xid ( dhcp->netdev ) ) {
		DBGC ( dhcp, "DHCP %p received %s %p has bad transaction ID\n",
		       dhcp, dhcp_msgtype_name ( msgtype ), dhcpset );
		rc = -EINVAL;
		goto err_xid;
	};

	/* Handle packet based on current state */
	switch ( dhcp->state ) {
	case DHCP_STATE_DISCOVER:
		if ( ( ( msgtype == DHCPOFFER ) || ( msgtype == DHCPNONE ) ) &&
		     ( src_port == htons ( BOOTPS_PORT ) ) )
			dhcp_rx_dhcpoffer ( dhcp, dhcpset );
		break;
	case DHCP_STATE_REQUEST:
		if ( ( ( msgtype == DHCPACK ) || ( msgtype == DHCPNONE ) ) &&
		     ( src_port == htons ( BOOTPS_PORT ) ) )
			dhcp_rx_dhcpack ( dhcp, dhcpset );
		break;
	case DHCP_STATE_PROXYREQUEST:
		if ( ( msgtype == DHCPACK ) &&
		     ( src_port == htons ( PROXYDHCP_PORT ) ) )
			dhcp_rx_proxydhcpack ( dhcp, dhcpset );
		break;
	default:
		assert ( 0 );
		break;
	}

 err_xid:
	dhcpset_put ( dhcpset );
 err_dhcpset_create:
 err_no_src:
 err_no_meta:
	free_iob ( iobuf );
	return rc;
}

/** DHCP data transfer interface operations */
static struct xfer_interface_operations dhcp_xfer_operations = {
	.close		= ignore_xfer_close,
	.vredirect	= xfer_vopen,
	.window		= unlimited_xfer_window,
	.alloc_iob	= default_xfer_alloc_iob,
	.deliver_iob	= dhcp_deliver_iob,
	.deliver_raw	= xfer_deliver_as_iob,
};

/**
 * Handle DHCP retry timer expiry
 *
 * @v timer		DHCP retry timer
 * @v fail		Failure indicator
 */
static void dhcp_timer_expired ( struct retry_timer *timer, int fail ) {
	struct dhcp_session *dhcp =
		container_of ( timer, struct dhcp_session, timer );
	unsigned long elapsed = ( currticks() - dhcp->start );

	/* If we have failed, terminate DHCP */
	if ( fail ) {
		dhcp_finished ( dhcp, -ETIMEDOUT );
		return;
	}

	/* Give up waiting for ProxyDHCP before we reach the failure point */
	if ( dhcp->dhcpoffer && ( elapsed > PROXYDHCP_WAIT_TIME ) ) {
		if ( dhcp->state == DHCP_STATE_DISCOVER ) {
			dhcp_set_state ( dhcp, DHCP_STATE_REQUEST );
			return;
		} else if ( dhcp->state == DHCP_STATE_PROXYREQUEST ) {
			dhcp_finished ( dhcp, 0 );
			return;
		}
	}

	/* Otherwise, retransmit current packet */
	dhcp_tx ( dhcp );
}

/****************************************************************************
 *
 * Job control interface
 *
 */

/**
 * Handle kill() event received via job control interface
 *
 * @v job		DHCP job control interface
 */
static void dhcp_job_kill ( struct job_interface *job ) {
	struct dhcp_session *dhcp =
		container_of ( job, struct dhcp_session, job );

	/* Terminate DHCP session */
	dhcp_finished ( dhcp, -ECANCELED );
}

/** DHCP job control interface operations */
static struct job_interface_operations dhcp_job_operations = {
	.done		= ignore_job_done,
	.kill		= dhcp_job_kill,
	.progress	= ignore_job_progress,
};

/****************************************************************************
 *
 * Instantiator
 *
 */

/**
 * Start DHCP on a network device
 *
 * @v job		Job control interface
 * @v netdev		Network device
 * @v register_options	DHCP option block registration routine
 * @ret rc		Return status code
 *
 * Starts DHCP on the specified network device.  If successful, the @c
 * register_options() routine will be called with the acquired
 * options.
 */
int start_dhcp ( struct job_interface *job, struct net_device *netdev ) {
	static struct sockaddr_in server = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = INADDR_BROADCAST,
		.sin_port = htons ( BOOTPS_PORT ),
	};
	static struct sockaddr_in client = {
		.sin_family = AF_INET,
		.sin_port = htons ( BOOTPC_PORT ),
	};
	struct dhcp_session *dhcp;
	int rc;

	/* Allocate and initialise structure */
	dhcp = zalloc ( sizeof ( *dhcp ) );
	if ( ! dhcp )
		return -ENOMEM;
	dhcp->refcnt.free = dhcp_free;
	job_init ( &dhcp->job, &dhcp_job_operations, &dhcp->refcnt );
	xfer_init ( &dhcp->xfer, &dhcp_xfer_operations, &dhcp->refcnt );
	dhcp->netdev = netdev_get ( netdev );
	dhcp->timer.expired = dhcp_timer_expired;
	dhcp->timer.min_timeout = DHCP_MIN_TIMEOUT;
	dhcp->timer.max_timeout = DHCP_MAX_TIMEOUT;
	dhcp->start = currticks();

	/* Instantiate child objects and attach to our interfaces */
	if ( ( rc = xfer_open_socket ( &dhcp->xfer, SOCK_DGRAM,
				       ( struct sockaddr * ) &server,
				       ( struct sockaddr * ) &client ) ) != 0 )
		goto err;

	/* Start timer to initiate initial DHCPREQUEST */
	start_timer_nodelay ( &dhcp->timer );

	/* Attach parent interface, mortalise self, and return */
	job_plug_plug ( &dhcp->job, job );
	ref_put ( &dhcp->refcnt );
	return 0;

 err:
	dhcp_finished ( dhcp, rc );
	ref_put ( &dhcp->refcnt );
	return rc;
}
