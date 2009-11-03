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

FILE_LICENCE ( GPL2_OR_LATER );

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
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
#include <gpxe/timer.h>
#include <gpxe/settings.h>
#include <gpxe/dhcp.h>
#include <gpxe/dhcpopts.h>
#include <gpxe/dhcppkt.h>
#include <gpxe/features.h>

/** @file
 *
 * Dynamic Host Configuration Protocol
 *
 */

struct dhcp_session;
static int dhcp_tx ( struct dhcp_session *dhcp );

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
	DHCP_MESSAGE_TYPE, DHCP_BYTE ( 0 ),
	DHCP_MAX_MESSAGE_SIZE,
	DHCP_WORD ( ETH_MAX_MTU - 20 /* IP header */ - 8 /* UDP header */ ),
	DHCP_CLIENT_ARCHITECTURE, DHCP_WORD ( 0 ),
	DHCP_CLIENT_NDI, DHCP_OPTION ( 1 /* UNDI */ , 2, 1 /* v2.1 */ ),
	DHCP_VENDOR_CLASS_ID,
	DHCP_STRING (  'P', 'X', 'E', 'C', 'l', 'i', 'e', 'n', 't', ':',
		       'A', 'r', 'c', 'h', ':', '0', '0', '0', '0', '0', ':',
		       'U', 'N', 'D', 'I', ':', '0', '0', '2', '0', '0', '1' ),
	DHCP_USER_CLASS_ID,
	DHCP_STRING ( 'g', 'P', 'X', 'E' ),
	DHCP_PARAMETER_REQUEST_LIST,
	DHCP_OPTION ( DHCP_SUBNET_MASK, DHCP_ROUTERS, DHCP_DNS_SERVERS,
		      DHCP_LOG_SERVERS, DHCP_HOST_NAME, DHCP_DOMAIN_NAME,
		      DHCP_ROOT_PATH, DHCP_VENDOR_ENCAP, DHCP_VENDOR_CLASS_ID,
		      DHCP_TFTP_SERVER_NAME, DHCP_BOOTFILE_NAME,
		      DHCP_EB_ENCAP, DHCP_ISCSI_INITIATOR_IQN ),
	DHCP_END
};

/** Version number feature */
FEATURE_VERSION ( VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH );

/** DHCP server address setting */
struct setting dhcp_server_setting __setting = {
	.name = "dhcp-server",
	.description = "DHCP server address",
	.tag = DHCP_SERVER_IDENTIFIER,
	.type = &setting_type_ipv4,
};

/** DHCP user class setting */
struct setting user_class_setting __setting = {
	.name = "user-class",
	.description = "User class identifier",
	.tag = DHCP_USER_CLASS_ID,
	.type = &setting_type_string,
};

/** Use cached network settings */
struct setting use_cached_setting __setting = {
	.name = "use-cached",
	.description = "Use cached network settings",
	.tag = DHCP_EB_USE_CACHED,
	.type = &setting_type_uint8,
};

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
 * DHCP session
 *
 */

struct dhcp_session;

/** DHCP session state operations */
struct dhcp_session_state {
	/** State name */
	const char *name;
	/**
	 * Construct transmitted packet
	 *
	 * @v dhcp		DHCP session
	 * @v dhcppkt		DHCP packet
	 * @v peer		Destination address
	 */
	int ( * tx ) ( struct dhcp_session *dhcp,
		       struct dhcp_packet *dhcppkt,
		       struct sockaddr_in *peer );
	/** Handle received packet
	 *
	 * @v dhcp		DHCP session
	 * @v dhcppkt		DHCP packet
	 * @v peer		DHCP server address
	 * @v msgtype		DHCP message type
	 * @v server_id		DHCP server ID
	 */
	void ( * rx ) ( struct dhcp_session *dhcp,
			struct dhcp_packet *dhcppkt,
			struct sockaddr_in *peer,
			uint8_t msgtype, struct in_addr server_id );
	/** Handle timer expiry
	 *
	 * @v dhcp		DHCP session
	 */
	void ( * expired ) ( struct dhcp_session *dhcp );
	/** Transmitted message type */
	uint8_t tx_msgtype;
	/** Apply minimum timeout */
	uint8_t apply_min_timeout;
};

static struct dhcp_session_state dhcp_state_discover;
static struct dhcp_session_state dhcp_state_request;
static struct dhcp_session_state dhcp_state_proxy;
static struct dhcp_session_state dhcp_state_pxebs;

/** DHCP offer is valid for IP lease */
#define DHCP_OFFER_IP	1

/** DHCP offer is valid for PXE options */
#define DHCP_OFFER_PXE	2

/** A DHCP offer */
struct dhcp_offer {
	/** IP address of server granting offer */
	struct in_addr server;

	/** IP address being offered, or 0.0.0.0 for a pure proxy */
	struct in_addr ip;

	/** DHCP packet containing PXE options; NULL if missing or proxied */
	struct dhcp_packet *pxe;

	/** Valid uses for this offer, a combination of DHCP_OFFER bits */
	uint8_t valid;

	/** Priority of this offer */
	int8_t priority;

	/** Whether to ignore PXE DHCP extensions */
	uint8_t no_pxedhcp;
};

/** Maximum number of DHCP offers to queue */
#define DHCP_MAX_OFFERS   6

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
	/** Local socket address */
	struct sockaddr_in local;
	/** State of the session */
	struct dhcp_session_state *state;

	/** PXE Boot Server type */
	uint16_t pxe_type;
	/** List of PXE Boot Servers to attempt */
	struct in_addr *pxe_attempt;
	/** List of PXE Boot Servers to accept */
	struct in_addr *pxe_accept;

	/** Retransmission timer */
	struct retry_timer timer;
	/** Start time of the current state (in ticks) */
	unsigned long start;

	/** DHCP offer just requested */
	struct dhcp_offer *current_offer;
	/** List of DHCP offers received */
	struct dhcp_offer offers[DHCP_MAX_OFFERS];
};

/**
 * Free DHCP session
 *
 * @v refcnt		Reference counter
 */
static void dhcp_free ( struct refcnt *refcnt ) {
	struct dhcp_session *dhcp =
		container_of ( refcnt, struct dhcp_session, refcnt );
	int i;

	for ( i = 0 ; i < DHCP_MAX_OFFERS ; i++ ) {
		if ( dhcp->offers[i].pxe )
			dhcppkt_put ( dhcp->offers[i].pxe );
	}

	netdev_put ( dhcp->netdev );
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

/**
 * Transition to new DHCP session state
 *
 * @v dhcp		DHCP session
 * @v state		New session state
 */
static void dhcp_set_state ( struct dhcp_session *dhcp,
			     struct dhcp_session_state *state ) {

	DBGC ( dhcp, "DHCP %p entering %s state\n", dhcp, state->name );
	dhcp->state = state;
	dhcp->start = currticks();
	stop_timer ( &dhcp->timer );
	dhcp->timer.min_timeout =
		( state->apply_min_timeout ? DHCP_MIN_TIMEOUT : 0 );
	dhcp->timer.max_timeout = DHCP_MAX_TIMEOUT;
	start_timer_nodelay ( &dhcp->timer );
}

/**
 * Determine next DHCP offer to try
 *
 * @v dhcp		DHCP session
 * @v type		DHCP offer type
 * @ret offer		Next DHCP offer to try
 *
 * Offers are ranked by priority, then by completeness (combined
 * IP+PXE are tried before @a type alone), then by order of receipt.
 */
static struct dhcp_offer * dhcp_next_offer ( struct dhcp_session *dhcp,
					     uint8_t type ) {

	struct dhcp_offer *offer;
	struct dhcp_offer *best = NULL;

	for ( offer = dhcp->offers ; offer < dhcp->offers + DHCP_MAX_OFFERS ;
	      offer++ ) {
		if ( ( offer->valid & type ) &&
		     ( ( best == NULL ) ||
		       ( offer->priority > best->priority ) ||
		       ( ( offer->priority == best->priority ) &&
			 ( offer->valid & ~best->valid ) ) ) )
			best = offer;
	}

	return best;
}

/****************************************************************************
 *
 * DHCP state machine
 *
 */

/**
 * Construct transmitted packet for DHCP discovery
 *
 * @v dhcp		DHCP session
 * @v dhcppkt		DHCP packet
 * @v peer		Destination address
 */
static int dhcp_discovery_tx ( struct dhcp_session *dhcp,
			       struct dhcp_packet *dhcppkt __unused,
			       struct sockaddr_in *peer ) {

	DBGC ( dhcp, "DHCP %p DHCPDISCOVER\n", dhcp );

	/* Set server address */
	peer->sin_addr.s_addr = INADDR_BROADCAST;
	peer->sin_port = htons ( BOOTPS_PORT );

	return 0;
}

/**
 * Handle received DHCPOFFER during any state
 *
 * @v dhcp		DHCP session
 * @v dhcppkt		DHCP packet
 * @v peer		DHCP server address
 * @v msgtype		DHCP message type
 * @v server_id		DHCP server ID
 */
static void dhcp_rx_offer ( struct dhcp_session *dhcp,
			    struct dhcp_packet *dhcppkt,
			    struct sockaddr_in *peer, uint8_t msgtype,
			    struct in_addr server_id ) {
	char vci[9]; /* "PXEClient" */
	int vci_len;
	int has_pxeclient;
	int pxeopts_len;
	int has_pxeopts;
	struct dhcp_offer *offer;
	int i;

	DBGC ( dhcp, "DHCP %p %s from %s:%d", dhcp,
	       dhcp_msgtype_name ( msgtype ), inet_ntoa ( peer->sin_addr ),
	       ntohs ( peer->sin_port ) );
	if ( server_id.s_addr != peer->sin_addr.s_addr )
		DBGC ( dhcp, " (%s)", inet_ntoa ( server_id ) );

	/* Identify offered IP address */
	if ( dhcppkt->dhcphdr->yiaddr.s_addr )
		DBGC ( dhcp, " for %s", inet_ntoa ( dhcppkt->dhcphdr->yiaddr ));

	/* Enqueue an offer to be filled in */
	for ( i = 0 ; i < DHCP_MAX_OFFERS ; i++ ) {
		if ( dhcp->offers[i].server.s_addr == server_id.s_addr ) {
			DBGC ( dhcp, " dup\n" );
			return;
		}

		if ( ! dhcp->offers[i].valid )
			break;
	}
	if ( i == DHCP_MAX_OFFERS ) {
		DBGC ( dhcp, " dropped\n" );
		return;
	}

	offer = &dhcp->offers[i];
	offer->server = server_id;
	offer->ip = dhcppkt->dhcphdr->yiaddr;

	/* Identify "PXEClient" vendor class */
	vci_len = dhcppkt_fetch ( dhcppkt, DHCP_VENDOR_CLASS_ID,
				  vci, sizeof ( vci ) );
	has_pxeclient = ( ( vci_len >= ( int ) sizeof ( vci ) ) &&
			  ( strncmp ( "PXEClient", vci, sizeof (vci) ) == 0 ));

	/* Identify presence of PXE-specific options */
	pxeopts_len = dhcppkt_fetch ( dhcppkt, DHCP_PXE_BOOT_MENU, NULL, 0 );
	has_pxeopts = ( pxeopts_len >= 0 );
	if ( has_pxeclient )
		DBGC ( dhcp, "%s", ( has_pxeopts ? " pxe" : " proxy" ) );

	if ( has_pxeclient && has_pxeopts ) {
		/* Save reference to packet for future use */
		if ( offer->pxe )
			dhcppkt_put ( offer->pxe );
		offer->pxe = dhcppkt_get ( dhcppkt );
	}

	/* Identify priority */
	dhcppkt_fetch ( dhcppkt, DHCP_EB_PRIORITY, &offer->priority,
			sizeof ( offer->priority ) );
	if ( offer->priority )
		DBGC ( dhcp, " pri %d", offer->priority );

	/* Identify ignore-PXE flag */
	dhcppkt_fetch ( dhcppkt, DHCP_EB_NO_PXEDHCP, &offer->no_pxedhcp,
			sizeof ( offer->no_pxedhcp ) );
	if ( offer->no_pxedhcp )
		DBGC ( dhcp, " nopxe" );
	DBGC ( dhcp, "\n" );

	/* Determine roles this offer can fill */
	if ( offer->ip.s_addr &&
	     ( peer->sin_port == htons ( BOOTPS_PORT ) ) &&
	     ( ( msgtype == DHCPOFFER ) || ( ! msgtype /* BOOTP */ ) ) )
		offer->valid |= DHCP_OFFER_IP;

	if ( has_pxeclient && ( msgtype == DHCPOFFER ) )
		offer->valid |= DHCP_OFFER_PXE;
}

/**
 * Handle received packet during DHCP discovery
 *
 * @v dhcp		DHCP session
 * @v dhcppkt		DHCP packet
 * @v peer		DHCP server address
 * @v msgtype		DHCP message type
 * @v server_id		DHCP server ID
 */
static void dhcp_discovery_rx ( struct dhcp_session *dhcp,
				struct dhcp_packet *dhcppkt,
				struct sockaddr_in *peer, uint8_t msgtype,
				struct in_addr server_id ) {
	unsigned long elapsed;
	struct dhcp_offer *ip_offer;

	dhcp_rx_offer ( dhcp, dhcppkt, peer, msgtype, server_id );

	/* We can exit the discovery state when we have a valid
	 * DHCPOFFER, and either:
	 *
	 *  o  The DHCPOFFER instructs us to ignore ProxyDHCPOFFERs, or
	 *  o  We have a valid ProxyDHCPOFFER, or
	 *  o  We have allowed sufficient time for ProxyDHCPOFFERs.
	 */

	/* If we don't yet have a DHCPOFFER, do nothing */
	ip_offer = dhcp_next_offer ( dhcp, DHCP_OFFER_IP );
	if ( ! ip_offer )
		return;

	/* If we can't yet transition to DHCPREQUEST, do nothing */
	elapsed = ( currticks() - dhcp->start );
	if ( ! ( ip_offer->no_pxedhcp ||
		 dhcp_next_offer ( dhcp, DHCP_OFFER_PXE ) ||
		 ( elapsed > PROXYDHCP_MAX_TIMEOUT ) ) )
		return;

	/* Transition to DHCPREQUEST */
	dhcp_set_state ( dhcp, &dhcp_state_request );
}

/**
 * Handle timer expiry during DHCP discovery
 *
 * @v dhcp		DHCP session
 */
static void dhcp_discovery_expired ( struct dhcp_session *dhcp ) {
	unsigned long elapsed = ( currticks() - dhcp->start );

	/* Give up waiting for ProxyDHCP before we reach the failure point */
	if ( dhcp_next_offer ( dhcp, DHCP_OFFER_IP ) &&
	     ( elapsed > PROXYDHCP_MAX_TIMEOUT ) ) {
		dhcp_set_state ( dhcp, &dhcp_state_request );
		return;
	}

	/* Otherwise, retransmit current packet */
	dhcp_tx ( dhcp );
}

/** DHCP discovery state operations */
static struct dhcp_session_state dhcp_state_discover = {
	.name			= "discovery",
	.tx			= dhcp_discovery_tx,
	.rx			= dhcp_discovery_rx,
	.expired		= dhcp_discovery_expired,
	.tx_msgtype		= DHCPDISCOVER,
	.apply_min_timeout	= 1,
};

/**
 * Construct transmitted packet for DHCP request
 *
 * @v dhcp		DHCP session
 * @v dhcppkt		DHCP packet
 * @v peer		Destination address
 */
static int dhcp_request_tx ( struct dhcp_session *dhcp,
			     struct dhcp_packet *dhcppkt,
			     struct sockaddr_in *peer ) {
	int rc;
	struct dhcp_offer *offer;

	offer = dhcp->current_offer = dhcp_next_offer ( dhcp, DHCP_OFFER_IP );

	DBGC ( dhcp, "DHCP %p DHCPREQUEST to %s:%d",
	       dhcp, inet_ntoa ( offer->server ), BOOTPS_PORT );
	DBGC ( dhcp, " for %s\n", inet_ntoa ( offer->ip ) );

	/* Set server ID */
	if ( ( rc = dhcppkt_store ( dhcppkt, DHCP_SERVER_IDENTIFIER,
				    &offer->server,
				    sizeof ( offer->server ) ) ) != 0 )
		return rc;

	/* Set requested IP address */
	if ( ( rc = dhcppkt_store ( dhcppkt, DHCP_REQUESTED_ADDRESS,
				    &offer->ip, sizeof ( offer->ip ) ) ) != 0 )
		return rc;

	/* Set server address */
	peer->sin_addr.s_addr = INADDR_BROADCAST;
	peer->sin_port = htons ( BOOTPS_PORT );

	return 0;
}

/**
 * Handle received packet during DHCP request
 *
 * @v dhcp		DHCP session
 * @v dhcppkt		DHCP packet
 * @v peer		DHCP server address
 * @v msgtype		DHCP message type
 * @v server_id		DHCP server ID
 */
static void dhcp_request_rx ( struct dhcp_session *dhcp,
			      struct dhcp_packet *dhcppkt,
			      struct sockaddr_in *peer, uint8_t msgtype,
			      struct in_addr server_id ) {
	struct in_addr ip;
	struct settings *parent;
	int rc;
	struct dhcp_offer *pxe_offer;

	if ( msgtype == DHCPOFFER ) {
		dhcp_rx_offer ( dhcp, dhcppkt, peer, msgtype, server_id );
		if ( dhcp_next_offer ( dhcp, DHCP_OFFER_IP ) !=
		     dhcp->current_offer ) {
			/* Restart due to higher-priority offer received */
			DBGC ( dhcp, "DHCP %p re-requesting\n", dhcp );
			dhcp_set_state ( dhcp, &dhcp_state_request );
		}
		return;
	}

	DBGC ( dhcp, "DHCP %p %s from %s:%d", dhcp,
	       dhcp_msgtype_name ( msgtype ), inet_ntoa ( peer->sin_addr ),
	       ntohs ( peer->sin_port ) );
	if ( server_id.s_addr != peer->sin_addr.s_addr )
		DBGC ( dhcp, " (%s)", inet_ntoa ( server_id ) );

	/* Identify leased IP address */
	ip = dhcppkt->dhcphdr->yiaddr;
	if ( ip.s_addr )
		DBGC ( dhcp, " for %s", inet_ntoa ( ip ) );
	DBGC ( dhcp, "\n" );

	/* Filter out unacceptable responses */
	if ( peer->sin_port != htons ( BOOTPS_PORT ) )
		return;
	if ( msgtype /* BOOTP */ && ( msgtype != DHCPACK ) )
		return;
	if ( server_id.s_addr != dhcp->current_offer->server.s_addr )
		return;

	/* Record assigned address */
	dhcp->local.sin_addr = ip;

	/* Register settings */
	parent = netdev_settings ( dhcp->netdev );
	if ( ( rc = register_settings ( &dhcppkt->settings, parent ) ) != 0 ){
		DBGC ( dhcp, "DHCP %p could not register settings: %s\n",
		       dhcp, strerror ( rc ) );
		dhcp_finished ( dhcp, rc );
		return;
	}

	/* Locate best source of PXE settings */
	pxe_offer = dhcp_next_offer ( dhcp, DHCP_OFFER_PXE );

	if ( ( ! pxe_offer ) || /* No PXE available */
	     /* IP offer instructs us to ignore PXE */
	     dhcp->current_offer->no_pxedhcp ||
	     /* PXE settings already registered with IP offer */
	     ( ( dhcp->current_offer == pxe_offer ) && ( pxe_offer->pxe ) ) ) {

		/* Terminate DHCP */
		dhcp_finished ( dhcp, 0 );

	} else if ( pxe_offer->pxe ) {
		/* Register PXE settings and terminate DHCP */
		pxe_offer->pxe->settings.name = PROXYDHCP_SETTINGS_NAME;
		if ( ( rc = register_settings ( &pxe_offer->pxe->settings,
						NULL ) ) != 0 ) {
			DBGC ( dhcp, "DHCP %p could not register settings: "
			       "%s\n", dhcp, strerror ( rc ) );
		}
		dhcp_finished ( dhcp, rc );
	} else {
		/* Start ProxyDHCP */
		dhcp_set_state ( dhcp, &dhcp_state_proxy );
	}
}

/**
 * Handle timer expiry during DHCP discovery
 *
 * @v dhcp		DHCP session
 */
static void dhcp_request_expired ( struct dhcp_session *dhcp ) {

	/* Retransmit current packet */
	dhcp_tx ( dhcp );
}

/** DHCP request state operations */
static struct dhcp_session_state dhcp_state_request = {
	.name			= "request",
	.tx			= dhcp_request_tx,
	.rx			= dhcp_request_rx,
	.expired		= dhcp_request_expired,
	.tx_msgtype		= DHCPREQUEST,
	.apply_min_timeout	= 0,
};

/**
 * Construct transmitted packet for ProxyDHCP request
 *
 * @v dhcp		DHCP session
 * @v dhcppkt		DHCP packet
 * @v peer		Destination address
 */
static int dhcp_proxy_tx ( struct dhcp_session *dhcp,
			   struct dhcp_packet *dhcppkt,
			   struct sockaddr_in *peer ) {
	int rc;
	struct dhcp_offer *offer;

	offer = dhcp->current_offer = dhcp_next_offer ( dhcp, DHCP_OFFER_PXE );

	DBGC ( dhcp, "DHCP %p ProxyDHCP REQUEST to %s:%d\n", dhcp,
	       inet_ntoa ( offer->server ), PXE_PORT );

	/* Set server ID */
	if ( ( rc = dhcppkt_store ( dhcppkt, DHCP_SERVER_IDENTIFIER,
				    &offer->server,
				    sizeof ( offer->server ) ) )  != 0 )
		return rc;

	/* Set server address */
	peer->sin_addr = offer->server;
	peer->sin_port = htons ( PXE_PORT );

	return 0;
}

/**
 * Handle received packet during ProxyDHCP request
 *
 * @v dhcp		DHCP session
 * @v dhcppkt		DHCP packet
 * @v peer		DHCP server address
 * @v msgtype		DHCP message type
 * @v server_id		DHCP server ID
 */
static void dhcp_proxy_rx ( struct dhcp_session *dhcp,
			    struct dhcp_packet *dhcppkt,
			    struct sockaddr_in *peer, uint8_t msgtype,
			    struct in_addr server_id ) {
	int rc;

	/* Enqueue last-minute DHCPOFFERs for use in case of failure */
	if ( peer->sin_port == htons ( BOOTPS_PORT ) &&
	     msgtype == DHCPOFFER ) {
		dhcp_rx_offer ( dhcp, dhcppkt, peer, msgtype, server_id );
		return;
	}

	DBGC ( dhcp, "DHCP %p %s from %s:%d", dhcp,
	       dhcp_msgtype_name ( msgtype ), inet_ntoa ( peer->sin_addr ),
	       ntohs ( peer->sin_port ) );
	if ( server_id.s_addr != peer->sin_addr.s_addr )
		DBGC ( dhcp, " (%s)", inet_ntoa ( server_id ) );
	DBGC ( dhcp, "\n" );

	/* Filter out unacceptable responses */
	if ( peer->sin_port != htons ( PXE_PORT ) )
		return;
	if ( msgtype != DHCPACK && msgtype != DHCPOFFER )
		return;
	if ( server_id.s_addr /* Linux PXE server omits server ID */ &&
	     ( server_id.s_addr != dhcp->current_offer->server.s_addr ) )
		return;

	/* Register settings */
	dhcppkt->settings.name = PROXYDHCP_SETTINGS_NAME;
	if ( ( rc = register_settings ( &dhcppkt->settings, NULL ) ) != 0 ) {
		DBGC ( dhcp, "DHCP %p could not register settings: %s\n",
		       dhcp, strerror ( rc ) );
		dhcp_finished ( dhcp, rc );
		return;
	}

	/* Terminate DHCP */
	dhcp_finished ( dhcp, 0 );
}

/**
 * Handle timer expiry during ProxyDHCP request
 *
 * @v dhcp		DHCP session
 */
static void dhcp_proxy_expired ( struct dhcp_session *dhcp ) {
	unsigned long elapsed = ( currticks() - dhcp->start );

	/* Give up waiting for ProxyDHCP before we reach the failure point */
	if ( elapsed > PROXYDHCP_MAX_TIMEOUT ) {

		/* Mark failed offer as unsuitable for ProxyDHCP */
		dhcp->current_offer->valid &= ~DHCP_OFFER_PXE;

		/* Prefer not to use only half of a PXE+IP offer if we
		 * have other offers available
		 */
		dhcp->current_offer->priority = -1;

		/* If we have any other PXE offers we can try, go back
		 * to DHCPREQUEST (since they might not be proxied
		 * offers, or might be coupled to a new IP address).
		 * We should probably DHCPRELEASE our old IP, but the
		 * standard does not require it.
		 */
		if ( dhcp_next_offer ( dhcp, DHCP_OFFER_PXE ) ) {
			dhcp->local.sin_addr.s_addr = 0;
			dhcp_set_state ( dhcp, &dhcp_state_request );
			return;
		}

		/* No possibilities left; finish without PXE options */
		dhcp_finished ( dhcp, 0 );
		return;
	}

	/* Retransmit current packet */
	dhcp_tx ( dhcp );
}

/** ProxyDHCP request state operations */
static struct dhcp_session_state dhcp_state_proxy = {
	.name			= "ProxyDHCP",
	.tx			= dhcp_proxy_tx,
	.rx			= dhcp_proxy_rx,
	.expired		= dhcp_proxy_expired,
	.tx_msgtype		= DHCPREQUEST,
	.apply_min_timeout	= 0,
};

/**
 * Construct transmitted packet for PXE Boot Server Discovery
 *
 * @v dhcp		DHCP session
 * @v dhcppkt		DHCP packet
 * @v peer		Destination address
 */
static int dhcp_pxebs_tx ( struct dhcp_session *dhcp,
			   struct dhcp_packet *dhcppkt,
			   struct sockaddr_in *peer ) {
	struct dhcp_pxe_boot_menu_item menu_item = { 0, 0 };
	int rc;

	/* Set server address */
	peer->sin_addr = *(dhcp->pxe_attempt);
	peer->sin_port = ( ( peer->sin_addr.s_addr == INADDR_BROADCAST ) ?
			   htons ( BOOTPS_PORT ) : htons ( PXE_PORT ) );

	DBGC ( dhcp, "DHCP %p PXEBS REQUEST to %s:%d for type %d\n",
	       dhcp, inet_ntoa ( peer->sin_addr ), ntohs ( peer->sin_port ),
	       le16_to_cpu ( dhcp->pxe_type ) );

	/* Set boot menu item */
	menu_item.type = dhcp->pxe_type;
	if ( ( rc = dhcppkt_store ( dhcppkt, DHCP_PXE_BOOT_MENU_ITEM,
				    &menu_item, sizeof ( menu_item ) ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Check to see if PXE Boot Server address is acceptable
 *
 * @v dhcp		DHCP session
 * @v bs		Boot Server address
 * @ret accept		Boot Server is acceptable
 */
static int dhcp_pxebs_accept ( struct dhcp_session *dhcp,
			       struct in_addr bs ) {
	struct in_addr *accept;

	/* Accept if we have no acceptance filter */
	if ( ! dhcp->pxe_accept )
		return 1;

	/* Scan through acceptance list */
	for ( accept = dhcp->pxe_accept ; accept->s_addr ; accept++ ) {
		if ( accept->s_addr == bs.s_addr )
			return 1;
	}

	DBGC ( dhcp, "DHCP %p rejecting server %s\n",
	       dhcp, inet_ntoa ( bs ) );
	return 0;
}

/**
 * Handle received packet during PXE Boot Server Discovery
 *
 * @v dhcp		DHCP session
 * @v dhcppkt		DHCP packet
 * @v peer		DHCP server address
 * @v msgtype		DHCP message type
 * @v server_id		DHCP server ID
 */
static void dhcp_pxebs_rx ( struct dhcp_session *dhcp,
			    struct dhcp_packet *dhcppkt,
			    struct sockaddr_in *peer, uint8_t msgtype,
			    struct in_addr server_id ) {
	struct dhcp_pxe_boot_menu_item menu_item = { 0, 0 };
	int rc;

	DBGC ( dhcp, "DHCP %p %s from %s:%d", dhcp,
	       dhcp_msgtype_name ( msgtype ), inet_ntoa ( peer->sin_addr ),
	       ntohs ( peer->sin_port ) );
	if ( server_id.s_addr != peer->sin_addr.s_addr )
		DBGC ( dhcp, " (%s)", inet_ntoa ( server_id ) );

	/* Identify boot menu item */
	dhcppkt_fetch ( dhcppkt, DHCP_PXE_BOOT_MENU_ITEM,
			&menu_item, sizeof ( menu_item ) );
	if ( menu_item.type )
		DBGC ( dhcp, " for type %d", ntohs ( menu_item.type ) );
	DBGC ( dhcp, "\n" );

	/* Filter out unacceptable responses */
	if ( ( peer->sin_port != htons ( BOOTPS_PORT ) ) &&
	     ( peer->sin_port != htons ( PXE_PORT ) ) )
		return;
	if ( msgtype != DHCPACK )
		return;
	if ( menu_item.type != dhcp->pxe_type )
		return;
	if ( ! dhcp_pxebs_accept ( dhcp, ( server_id.s_addr ?
					   server_id : peer->sin_addr ) ) )
		return;

	/* Register settings */
	dhcppkt->settings.name = PXEBS_SETTINGS_NAME;
	if ( ( rc = register_settings ( &dhcppkt->settings, NULL ) ) != 0 ) {
		DBGC ( dhcp, "DHCP %p could not register settings: %s\n",
		       dhcp, strerror ( rc ) );
		dhcp_finished ( dhcp, rc );
		return;
	}

	/* Terminate DHCP */
	dhcp_finished ( dhcp, 0 );
}

/**
 * Handle timer expiry during PXE Boot Server Discovery
 *
 * @v dhcp		DHCP session
 */
static void dhcp_pxebs_expired ( struct dhcp_session *dhcp ) {
	unsigned long elapsed = ( currticks() - dhcp->start );

	/* Give up waiting before we reach the failure point, and fail
	 * over to the next server in the attempt list
	 */
	if ( elapsed > PXEBS_MAX_TIMEOUT ) {
		dhcp->pxe_attempt++;
		if ( dhcp->pxe_attempt->s_addr ) {
			dhcp_set_state ( dhcp, &dhcp_state_pxebs );
			return;
		} else {
			dhcp_finished ( dhcp, -ETIMEDOUT );
			return;
		}
	}

	/* Retransmit current packet */
	dhcp_tx ( dhcp );
}

/** PXE Boot Server Discovery state operations */
static struct dhcp_session_state dhcp_state_pxebs = {
	.name			= "PXEBS",
	.tx			= dhcp_pxebs_tx,
	.rx			= dhcp_pxebs_rx,
	.expired		= dhcp_pxebs_expired,
	.tx_msgtype		= DHCPREQUEST,
	.apply_min_timeout	= 1,
};

/****************************************************************************
 *
 * Packet construction
 *
 */

/**
 * Construct DHCP client hardware address field and broadcast flag
 *
 * @v netdev		Network device
 * @v hlen		DHCP hardware address length to fill in
 * @v flags		DHCP flags to fill in
 * @ret chaddr		DHCP client hardware address
 */
void * dhcp_chaddr ( struct net_device *netdev, uint8_t *hlen,
		     uint16_t *flags ) {
	struct ll_protocol *ll_protocol = netdev->ll_protocol;
	typeof ( ( ( struct dhcphdr * ) NULL )->chaddr ) chaddr;

	/* If the link-layer address cannot fit into the chaddr field
	 * (as is the case for IPoIB) then try using the hardware
	 * address instead.  If we do this, set the broadcast flag,
	 * since chaddr then does not represent a valid link-layer
	 * address for the return path.
	 *
	 * If even the hardware address is too large, use an empty
	 * chaddr field and set the broadcast flag.
	 *
	 * This goes against RFC4390, but RFC4390 mandates that we use
	 * a DHCP client identifier that conforms with RFC4361, which
	 * we cannot do without either persistent (NIC-independent)
	 * storage, or by eliminating the hardware address completely
	 * from the DHCP packet, which seems unfriendly to users.
	 */
	if ( ( *hlen = ll_protocol->ll_addr_len ) <= sizeof ( chaddr ) ) {
		return netdev->ll_addr;
	}
	*flags = htons ( BOOTP_FL_BROADCAST );
	if ( ( *hlen = ll_protocol->hw_addr_len ) <= sizeof ( chaddr ) ) {
		return netdev->hw_addr;
	} else {
		*hlen = 0;
		return NULL;
	}
}

/**
 * Create a DHCP packet
 *
 * @v dhcppkt		DHCP packet structure to fill in
 * @v netdev		Network device
 * @v msgtype		DHCP message type
 * @v options		Initial options to include (or NULL)
 * @v options_len	Length of initial options
 * @v data		Buffer for DHCP packet
 * @v max_len		Size of DHCP packet buffer
 * @ret rc		Return status code
 *
 * Creates a DHCP packet in the specified buffer, and initialise a
 * DHCP packet structure.
 */
int dhcp_create_packet ( struct dhcp_packet *dhcppkt,
			 struct net_device *netdev, uint8_t msgtype,
			 const void *options, size_t options_len,
			 void *data, size_t max_len ) {
	struct dhcphdr *dhcphdr = data;
	void *chaddr;
	int rc;

	/* Sanity check */
	if ( max_len < ( sizeof ( *dhcphdr ) + options_len ) )
		return -ENOSPC;

	/* Initialise DHCP packet content */
	memset ( dhcphdr, 0, max_len );
	dhcphdr->xid = dhcp_xid ( netdev );
	dhcphdr->magic = htonl ( DHCP_MAGIC_COOKIE );
	dhcphdr->htype = ntohs ( netdev->ll_protocol->ll_proto );
	dhcphdr->op = dhcp_op[msgtype];
	chaddr = dhcp_chaddr ( netdev, &dhcphdr->hlen, &dhcphdr->flags );
	memcpy ( dhcphdr->chaddr, chaddr, dhcphdr->hlen );
	memcpy ( dhcphdr->options, options, options_len );

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
 * @v msgtype		DHCP message type
 * @v ciaddr		Client IP address
 * @v data		Buffer for DHCP packet
 * @v max_len		Size of DHCP packet buffer
 * @ret rc		Return status code
 *
 * Creates a DHCP request packet in the specified buffer, and
 * initialise a DHCP packet structure.
 */
int dhcp_create_request ( struct dhcp_packet *dhcppkt,
			  struct net_device *netdev, unsigned int msgtype,
			  struct in_addr ciaddr, void *data, size_t max_len ) {
	struct dhcp_netdev_desc dhcp_desc;
	struct dhcp_client_id client_id;
	struct dhcp_client_uuid client_uuid;
	uint8_t *dhcp_features;
	size_t dhcp_features_len;
	size_t ll_addr_len;
	ssize_t len;
	int rc;

	/* Create DHCP packet */
	if ( ( rc = dhcp_create_packet ( dhcppkt, netdev, msgtype,
					 dhcp_request_options_data,
					 sizeof ( dhcp_request_options_data ),
					 data, max_len ) ) != 0 ) {
		DBG ( "DHCP could not create DHCP packet: %s\n",
		      strerror ( rc ) );
		return rc;
	}

	/* Set client IP address */
	dhcppkt->dhcphdr->ciaddr = ciaddr;

	/* Add options to identify the feature list */
	dhcp_features = table_start ( DHCP_FEATURES );
	dhcp_features_len = table_num_entries ( DHCP_FEATURES );
	if ( ( rc = dhcppkt_store ( dhcppkt, DHCP_EB_ENCAP, dhcp_features,
				    dhcp_features_len ) ) != 0 ) {
		DBG ( "DHCP could not set features list option: %s\n",
		      strerror ( rc ) );
		return rc;
	}

	/* Add options to identify the network device */
	fetch_setting ( &netdev->settings.settings, &busid_setting, &dhcp_desc,
		sizeof ( dhcp_desc ) );
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
	if ( ( len = fetch_uuid_setting ( NULL, &uuid_setting,
					  &client_uuid.uuid ) ) >= 0 ) {
		if ( ( rc = dhcppkt_store ( dhcppkt, DHCP_CLIENT_UUID,
					    &client_uuid,
					    sizeof ( client_uuid ) ) ) != 0 ) {
			DBG ( "DHCP could not set client UUID: %s\n",
			      strerror ( rc ) );
			return rc;
		}
	}

	/* Add user class, if we have one. */
	if ( ( len = fetch_setting_len ( NULL, &user_class_setting ) ) >= 0 ) {
		char user_class[len];
		fetch_setting ( NULL, &user_class_setting, user_class,
				sizeof ( user_class ) );
		if ( ( rc = dhcppkt_store ( dhcppkt, DHCP_USER_CLASS_ID,
					    &user_class,
					    sizeof ( user_class ) ) ) != 0 ) {
			DBG ( "DHCP could not set user class: %s\n",
			      strerror ( rc ) );
			return rc;
		}
	}

	return 0;
}

/****************************************************************************
 *
 * Data transfer interface
 *
 */

/**
 * Transmit DHCP request
 *
 * @v dhcp		DHCP session
 * @ret rc		Return status code
 */
static int dhcp_tx ( struct dhcp_session *dhcp ) {
	static struct sockaddr_in peer = {
		.sin_family = AF_INET,
	};
	struct xfer_metadata meta = {
		.netdev = dhcp->netdev,
		.src = ( struct sockaddr * ) &dhcp->local,
		.dest = ( struct sockaddr * ) &peer,
	};
	struct io_buffer *iobuf;
	uint8_t msgtype = dhcp->state->tx_msgtype;
	struct dhcp_packet dhcppkt;
	int rc;

	/* Start retry timer.  Do this first so that failures to
	 * transmit will be retried.
	 */
	start_timer ( &dhcp->timer );

	/* Allocate buffer for packet */
	iobuf = xfer_alloc_iob ( &dhcp->xfer, DHCP_MIN_LEN );
	if ( ! iobuf )
		return -ENOMEM;

	/* Create basic DHCP packet in temporary buffer */
	if ( ( rc = dhcp_create_request ( &dhcppkt, dhcp->netdev, msgtype,
					  dhcp->local.sin_addr, iobuf->data,
					  iob_tailroom ( iobuf ) ) ) != 0 ) {
		DBGC ( dhcp, "DHCP %p could not construct DHCP request: %s\n",
		       dhcp, strerror ( rc ) );
		goto done;
	}

	/* Fill in packet based on current state */
	if ( ( rc = dhcp->state->tx ( dhcp, &dhcppkt, &peer ) ) != 0 ) {
		DBGC ( dhcp, "DHCP %p could not fill DHCP request: %s\n",
		       dhcp, strerror ( rc ) );
		goto done;
	}

	/* Transmit the packet */
	iob_put ( iobuf, dhcppkt.len );
	if ( ( rc = xfer_deliver_iob_meta ( &dhcp->xfer, iob_disown ( iobuf ),
					    &meta ) ) != 0 ) {
		DBGC ( dhcp, "DHCP %p could not transmit UDP packet: %s\n",
		       dhcp, strerror ( rc ) );
		goto done;
	}

 done:
	free_iob ( iobuf );
	return rc;
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
	struct sockaddr_in *peer;
	size_t data_len;
	struct dhcp_packet *dhcppkt;
	struct dhcphdr *dhcphdr;
	uint8_t msgtype = 0;
	struct in_addr server_id = { 0 };
	int rc = 0;

	/* Sanity checks */
	if ( ! meta->src ) {
		DBGC ( dhcp, "DHCP %p received packet without source port\n",
		       dhcp );
		rc = -EINVAL;
		goto err_no_src;
	}
	peer = ( struct sockaddr_in * ) meta->src;

	/* Create a DHCP packet containing the I/O buffer contents.
	 * Whilst we could just use the original buffer in situ, that
	 * would waste the unused space in the packet buffer, and also
	 * waste a relatively scarce fully-aligned I/O buffer.
	 */
	data_len = iob_len ( iobuf );
	dhcppkt = zalloc ( sizeof ( *dhcppkt ) + data_len );
	if ( ! dhcppkt ) {
		rc = -ENOMEM;
		goto err_alloc_dhcppkt;
	}
	dhcphdr = ( ( ( void * ) dhcppkt ) + sizeof ( *dhcppkt ) );
	memcpy ( dhcphdr, iobuf->data, data_len );
	dhcppkt_init ( dhcppkt, dhcphdr, data_len );

	/* Identify message type */
	dhcppkt_fetch ( dhcppkt, DHCP_MESSAGE_TYPE, &msgtype,
			sizeof ( msgtype ) );

	/* Identify server ID */
	dhcppkt_fetch ( dhcppkt, DHCP_SERVER_IDENTIFIER,
			&server_id, sizeof ( server_id ) );

	/* Check for matching transaction ID */
	if ( dhcphdr->xid != dhcp_xid ( dhcp->netdev ) ) {
		DBGC ( dhcp, "DHCP %p %s from %s:%d has bad transaction "
		       "ID\n", dhcp, dhcp_msgtype_name ( msgtype ),
		       inet_ntoa ( peer->sin_addr ),
		       ntohs ( peer->sin_port ) );
		rc = -EINVAL;
		goto err_xid;
	};

	/* Handle packet based on current state */
	dhcp->state->rx ( dhcp, dhcppkt, peer, msgtype, server_id );

 err_xid:
	dhcppkt_put ( dhcppkt );
 err_alloc_dhcppkt:
 err_no_src:
	free_iob ( iobuf );
	return rc;
}

/** DHCP data transfer interface operations */
static struct xfer_interface_operations dhcp_xfer_operations = {
	.close		= ignore_xfer_close,
	.vredirect	= xfer_vreopen,
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

	/* If we have failed, terminate DHCP */
	if ( fail ) {
		dhcp_finished ( dhcp, -ETIMEDOUT );
		return;
	}

	/* Handle timer expiry based on current state */
	dhcp->state->expired ( dhcp );
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
 * Instantiators
 *
 */

/**
 * DHCP peer address for socket opening
 *
 * This is a dummy address; the only useful portion is the socket
 * family (so that we get a UDP connection).  The DHCP client will set
 * the IP address and source port explicitly on each transmission.
 */
static struct sockaddr dhcp_peer = {
	.sa_family = AF_INET,
};

/**
 * Start DHCP state machine on a network device
 *
 * @v job		Job control interface
 * @v netdev		Network device
 * @ret rc		Return status code, or positive if cached
 *
 * Starts DHCP on the specified network device.  If successful, the
 * DHCPACK (and ProxyDHCPACK, if applicable) will be registered as
 * option sources.
 *
 * On a return of 0, a background job has been started to perform the
 * DHCP request. Any nonzero return means the job has not been
 * started; a positive return value indicates the success condition of
 * having fetched the appropriate data from cached information.
 */
int start_dhcp ( struct job_interface *job, struct net_device *netdev ) {
	struct dhcp_session *dhcp;
	int rc;

	/* Check for cached DHCP information */
	get_cached_dhcpack();
	if ( fetch_uintz_setting ( NULL, &use_cached_setting ) ) {
		DBG ( "DHCP using cached network settings\n" );
		return 1;
	}

	/* Allocate and initialise structure */
	dhcp = zalloc ( sizeof ( *dhcp ) );
	if ( ! dhcp )
		return -ENOMEM;
	dhcp->refcnt.free = dhcp_free;
	job_init ( &dhcp->job, &dhcp_job_operations, &dhcp->refcnt );
	xfer_init ( &dhcp->xfer, &dhcp_xfer_operations, &dhcp->refcnt );
	dhcp->netdev = netdev_get ( netdev );
	dhcp->local.sin_family = AF_INET;
	dhcp->local.sin_port = htons ( BOOTPC_PORT );
	dhcp->timer.expired = dhcp_timer_expired;

	/* Instantiate child objects and attach to our interfaces */
	if ( ( rc = xfer_open_socket ( &dhcp->xfer, SOCK_DGRAM, &dhcp_peer,
				  ( struct sockaddr * ) &dhcp->local ) ) != 0 )
		goto err;

	/* Enter DHCPDISCOVER state */
	dhcp_set_state ( dhcp, &dhcp_state_discover );

	/* Attach parent interface, mortalise self, and return */
	job_plug_plug ( &dhcp->job, job );
	ref_put ( &dhcp->refcnt );
	return 0;

 err:
	dhcp_finished ( dhcp, rc );
	ref_put ( &dhcp->refcnt );
	return rc;
}

/**
 * Retrieve list of PXE boot servers for a given server type
 *
 * @v dhcp		DHCP session
 * @v raw		DHCP PXE boot server list
 * @v raw_len		Length of DHCP PXE boot server list
 * @v ip		IP address list to fill in
 *
 * The caller must ensure that the IP address list has sufficient
 * space.
 */
static void pxebs_list ( struct dhcp_session *dhcp, void *raw,
			 size_t raw_len, struct in_addr *ip ) {
	struct dhcp_pxe_boot_server *server = raw;
	size_t server_len;
	unsigned int i;

	while ( raw_len ) {
		if ( raw_len < sizeof ( *server ) ) {
			DBGC ( dhcp, "DHCP %p malformed PXE server list\n",
			       dhcp );
			break;
		}
		server_len = offsetof ( typeof ( *server ),
					ip[ server->num_ip ] );
		if ( raw_len < server_len ) {
			DBGC ( dhcp, "DHCP %p malformed PXE server list\n",
			       dhcp );
			break;
		}
		if ( server->type == dhcp->pxe_type ) {
			for ( i = 0 ; i < server->num_ip ; i++ )
				*(ip++) = server->ip[i];
		}
		server = ( ( ( void * ) server ) + server_len );
		raw_len -= server_len;
	}
}

/**
 * Start PXE Boot Server Discovery on a network device
 *
 * @v job		Job control interface
 * @v netdev		Network device
 * @v pxe_type		PXE server type
 * @ret rc		Return status code
 *
 * Starts PXE Boot Server Discovery on the specified network device.
 * If successful, the Boot Server ACK will be registered as an option
 * source.
 */
int start_pxebs ( struct job_interface *job, struct net_device *netdev,
		  unsigned int pxe_type ) {
	struct setting pxe_discovery_control_setting =
		{ .tag = DHCP_PXE_DISCOVERY_CONTROL };
	struct setting pxe_boot_servers_setting =
		{ .tag = DHCP_PXE_BOOT_SERVERS };
	struct setting pxe_boot_server_mcast_setting =
		{ .tag = DHCP_PXE_BOOT_SERVER_MCAST };
	ssize_t pxebs_list_len;
	struct dhcp_session *dhcp;
	struct in_addr *ip;
	unsigned int pxe_discovery_control;
	int rc;

	/* Get upper bound for PXE boot server IP address list */
	pxebs_list_len = fetch_setting_len ( NULL, &pxe_boot_servers_setting );
	if ( pxebs_list_len < 0 )
		pxebs_list_len = 0;

	/* Allocate and initialise structure */
	dhcp = zalloc ( sizeof ( *dhcp ) + sizeof ( *ip ) /* mcast */ +
			sizeof ( *ip ) /* bcast */ + pxebs_list_len +
			sizeof ( *ip ) /* terminator */ );
	if ( ! dhcp )
		return -ENOMEM;
	dhcp->refcnt.free = dhcp_free;
	job_init ( &dhcp->job, &dhcp_job_operations, &dhcp->refcnt );
	xfer_init ( &dhcp->xfer, &dhcp_xfer_operations, &dhcp->refcnt );
	dhcp->netdev = netdev_get ( netdev );
	dhcp->local.sin_family = AF_INET;
	fetch_ipv4_setting ( netdev_settings ( netdev ), &ip_setting,
			     &dhcp->local.sin_addr );
	dhcp->local.sin_port = htons ( BOOTPC_PORT );
	dhcp->pxe_type = cpu_to_le16 ( pxe_type );
	dhcp->timer.expired = dhcp_timer_expired;

	/* Construct PXE boot server IP address lists */
	pxe_discovery_control =
		fetch_uintz_setting ( NULL, &pxe_discovery_control_setting );
	ip = ( ( ( void * ) dhcp ) + sizeof ( *dhcp ) );
	dhcp->pxe_attempt = ip;
	if ( ! ( pxe_discovery_control & PXEBS_NO_MULTICAST ) ) {
		fetch_ipv4_setting ( NULL, &pxe_boot_server_mcast_setting, ip);
		if ( ip->s_addr )
			ip++;
	}
	if ( ! ( pxe_discovery_control & PXEBS_NO_BROADCAST ) )
		(ip++)->s_addr = INADDR_BROADCAST;
	if ( pxe_discovery_control & PXEBS_NO_UNKNOWN_SERVERS )
		dhcp->pxe_accept = ip;
	if ( pxebs_list_len ) {
		uint8_t buf[pxebs_list_len];

		fetch_setting ( NULL, &pxe_boot_servers_setting,
				buf, sizeof ( buf ) );
		pxebs_list ( dhcp, buf, sizeof ( buf ), ip );
	}
	if ( ! dhcp->pxe_attempt->s_addr ) {
		DBGC ( dhcp, "DHCP %p has no PXE boot servers for type %04x\n",
		       dhcp, pxe_type );
		rc = -EINVAL;
		goto err;
	}

	/* Dump out PXE server lists */
	DBGC ( dhcp, "DHCP %p attempting", dhcp );
	for ( ip = dhcp->pxe_attempt ; ip->s_addr ; ip++ )
		DBGC ( dhcp, " %s", inet_ntoa ( *ip ) );
	DBGC ( dhcp, "\n" );
	if ( dhcp->pxe_accept ) {
		DBGC ( dhcp, "DHCP %p accepting", dhcp );
		for ( ip = dhcp->pxe_accept ; ip->s_addr ; ip++ )
			DBGC ( dhcp, " %s", inet_ntoa ( *ip ) );
		DBGC ( dhcp, "\n" );
	}

	/* Instantiate child objects and attach to our interfaces */
	if ( ( rc = xfer_open_socket ( &dhcp->xfer, SOCK_DGRAM, &dhcp_peer,
				  ( struct sockaddr * ) &dhcp->local ) ) != 0 )
		goto err;

	/* Enter PXEBS state */
	dhcp_set_state ( dhcp, &dhcp_state_pxebs );

	/* Attach parent interface, mortalise self, and return */
	job_plug_plug ( &dhcp->job, job );
	ref_put ( &dhcp->refcnt );
	return 0;

 err:
	dhcp_finished ( dhcp, rc );
	ref_put ( &dhcp->refcnt );
	return rc;
}
