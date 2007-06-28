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
#include <errno.h>
#include <assert.h>
#include <byteswap.h>
#include <gpxe/if_ether.h>
#include <gpxe/netdevice.h>
#include <gpxe/xfer.h>
#include <gpxe/open.h>
#include <gpxe/job.h>
#include <gpxe/retry.h>
#include <gpxe/tcpip.h>
#include <gpxe/ip.h>
#include <gpxe/dhcp.h>

/** @file
 *
 * Dynamic Host Configuration Protocol
 *
 */

/** DHCP operation types
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
	DHCP_MAX_MESSAGE_SIZE, DHCP_WORD ( ETH_MAX_MTU ),
	DHCP_VENDOR_CLASS_ID,
	DHCP_STRING (  'E', 't', 'h', 'e', 'r', 'b', 'o', 'o', 't' ),
	DHCP_PARAMETER_REQUEST_LIST,
	DHCP_OPTION ( DHCP_SUBNET_MASK, DHCP_ROUTERS, DHCP_DNS_SERVERS, DHCP_LOG_SERVERS,
		      DHCP_HOST_NAME, DHCP_DOMAIN_NAME, DHCP_ROOT_PATH,
		      DHCP_VENDOR_ENCAP, DHCP_TFTP_SERVER_NAME,
		      DHCP_BOOTFILE_NAME, DHCP_EB_ENCAP,
		      DHCP_ISCSI_INITIATOR_IQN ),
	DHCP_END
};

/**
 * Name a DHCP packet type
 *
 * @v msgtype		DHCP message type
 * @ret string		DHCP mesasge type name
 */
static inline const char * dhcp_msgtype_name ( unsigned int msgtype ) {
	switch ( msgtype ) {
	case 0:			return "BOOTP"; /* Non-DHCP packet */
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

/** Options common to all DHCP requests */
struct dhcp_option_block dhcp_request_options = {
	.data = dhcp_request_options_data,
	.max_len = sizeof ( dhcp_request_options_data ),
	.len = sizeof ( dhcp_request_options_data ),
};

/**
 * Set option within DHCP packet
 *
 * @v dhcppkt		DHCP packet
 * @v tag		DHCP option tag
 * @v data		New value for DHCP option
 * @v len		Length of value, in bytes
 * @ret rc		Return status code
 *
 * Sets the option within the first available options block within the
 * DHCP packet.  Option blocks are tried in the order specified by @c
 * dhcp_option_block_fill_order.
 *
 * The magic options @c DHCP_EB_YIADDR and @c DHCP_EB_SIADDR are
 * intercepted and inserted into the appropriate fixed fields within
 * the DHCP packet.  The option @c DHCP_OPTION_OVERLOAD is silently
 * ignored, since our DHCP packet assembly method relies on always
 * having option overloading in use.
 */
static int set_dhcp_packet_option ( struct dhcp_packet *dhcppkt,
				    unsigned int tag, const void *data,
				    size_t len ) {
	struct dhcphdr *dhcphdr = dhcppkt->dhcphdr;
	struct dhcp_option_block *options = dhcppkt->options;
	struct dhcp_option *option = NULL;

	/* Special-case the magic options */
	switch ( tag ) {
	case DHCP_OPTION_OVERLOAD:
		/* Hard-coded in packets we create; always ignore */
		return 0;
	case DHCP_EB_YIADDR:
		memcpy ( &dhcphdr->yiaddr, data, sizeof ( dhcphdr->yiaddr ) );
		return 0;
	case DHCP_EB_SIADDR:
		memcpy ( &dhcphdr->siaddr, data, sizeof ( dhcphdr->siaddr ) );
		return 0;
	case DHCP_MESSAGE_TYPE:
	case DHCP_REQUESTED_ADDRESS:
	case DHCP_PARAMETER_REQUEST_LIST:
		/* These options have to be within the main options
		 * block.  This doesn't seem to be required by the
		 * RFCs, but at least ISC dhcpd refuses to recognise
		 * them otherwise.
		 */
		options = &dhcppkt->options[OPTS_MAIN];
		break;
	default:
		/* Continue processing as normal */
		break;
	}
		
	/* Set option in first available options block */
	for ( ; options < &dhcppkt->options[NUM_OPT_BLOCKS] ; options++ ) {
		option = set_dhcp_option ( options, tag, data, len );
		if ( option )
			break;
	}

	/* Update DHCP packet length */
	dhcppkt->len = ( offsetof ( typeof ( *dhcppkt->dhcphdr ), options )
			 + dhcppkt->options[OPTS_MAIN].len );

	return ( option ? 0 : -ENOSPC );
}

/**
 * Copy option into DHCP packet
 *
 * @v dhcppkt		DHCP packet
 * @v options		DHCP option block, or NULL
 * @v tag		DHCP option tag to search for
 * @v new_tag		DHCP option tag to use for copied option
 * @ret rc		Return status code
 *
 * Copies a single option, if present, from the DHCP options block
 * into a DHCP packet.  The tag for the option may be changed if
 * desired; this is required by other parts of the DHCP code.
 *
 * @c options may specify a single options block, or be left as NULL
 * in order to search for the option within all registered options
 * blocks.
 */
static int copy_dhcp_packet_option ( struct dhcp_packet *dhcppkt,
				     struct dhcp_option_block *options,
				     unsigned int tag, unsigned int new_tag ) {
	struct dhcp_option *option;
	int rc;

	option = find_dhcp_option ( options, tag );
	if ( option ) {
		if ( ( rc = set_dhcp_packet_option ( dhcppkt, new_tag,
						     &option->data,
						     option->len ) ) != 0 )
			return rc;
	}
	return 0;
}

/**
 * Copy options into DHCP packet
 *
 * @v dhcppkt		DHCP packet
 * @v options		DHCP option block, or NULL
 * @v encapsulator	Encapsulating option, or zero
 * @ret rc		Return status code
 * 
 * Copies options with the specified encapsulator from DHCP options
 * blocks into a DHCP packet.  Most options are copied verbatim.
 * Recognised encapsulated options fields are handled as such.
 *
 * @c options may specify a single options block, or be left as NULL
 * in order to copy options from all registered options blocks.
 */
static int copy_dhcp_packet_encap_options ( struct dhcp_packet *dhcppkt,
					    struct dhcp_option_block *options,
					    unsigned int encapsulator ) {
	unsigned int subtag;
	unsigned int tag;
	int rc;

	for ( subtag = DHCP_MIN_OPTION; subtag <= DHCP_MAX_OPTION; subtag++ ) {
		tag = DHCP_ENCAP_OPT ( encapsulator, subtag );
		switch ( tag ) {
		case DHCP_EB_ENCAP:
		case DHCP_VENDOR_ENCAP:
			/* Process encapsulated options field */
			if ( ( rc = copy_dhcp_packet_encap_options ( dhcppkt,
								     options,
								     tag)) !=0)
				return rc;
			break;
		default:
			/* Copy option to reassembled packet */
			if ( ( rc = copy_dhcp_packet_option ( dhcppkt, options,
							      tag, tag ) ) !=0)
				return rc;
			break;
		};
	}

	return 0;
}

/**
 * Copy options into DHCP packet
 *
 * @v dhcppkt		DHCP packet
 * @v options		DHCP option block, or NULL
 * @ret rc		Return status code
 * 
 * Copies options from DHCP options blocks into a DHCP packet.  Most
 * options are copied verbatim.  Recognised encapsulated options
 * fields are handled as such.
 *
 * @c options may specify a single options block, or be left as NULL
 * in order to copy options from all registered options blocks.
 */
int copy_dhcp_packet_options ( struct dhcp_packet *dhcppkt,
			       struct dhcp_option_block *options ) {
	return copy_dhcp_packet_encap_options ( dhcppkt, options, 0 );
}

/**
 * Create a DHCP packet
 *
 * @v netdev		Network device
 * @v msgtype		DHCP message type
 * @v data		Buffer for DHCP packet
 * @v max_len		Size of DHCP packet buffer
 * @v dhcppkt		DHCP packet structure to fill in
 * @ret rc		Return status code
 *
 * Creates a DHCP packet in the specified buffer, and fills out a @c
 * dhcp_packet structure that can be passed to
 * set_dhcp_packet_option() or copy_dhcp_packet_options().
 */
int create_dhcp_packet ( struct net_device *netdev, uint8_t msgtype,
			 void *data, size_t max_len,
			 struct dhcp_packet *dhcppkt ) {
	struct dhcphdr *dhcphdr = data;
	static const uint8_t overloading = ( DHCP_OPTION_OVERLOAD_FILE |
					     DHCP_OPTION_OVERLOAD_SNAME );
	int rc;

	/* Sanity check */
	if ( max_len < sizeof ( *dhcphdr ) )
		return -ENOSPC;

	/* Initialise DHCP packet content */
	memset ( dhcphdr, 0, max_len );
	dhcphdr->xid = dhcp_xid ( netdev );
	dhcphdr->magic = htonl ( DHCP_MAGIC_COOKIE );
	dhcphdr->htype = ntohs ( netdev->ll_protocol->ll_proto );
	dhcphdr->hlen = netdev->ll_protocol->ll_addr_len;
	memcpy ( dhcphdr->chaddr, netdev->ll_addr, dhcphdr->hlen );
	dhcphdr->op = dhcp_op[msgtype];

	/* Initialise DHCP packet structure */
	dhcppkt->dhcphdr = dhcphdr;
	dhcppkt->max_len = max_len;
	init_dhcp_options ( &dhcppkt->options[OPTS_MAIN], dhcphdr->options,
			    ( max_len -
			      offsetof ( typeof ( *dhcphdr ), options ) ) );
	init_dhcp_options ( &dhcppkt->options[OPTS_FILE], dhcphdr->file,
			    sizeof ( dhcphdr->file ) );
	init_dhcp_options ( &dhcppkt->options[OPTS_SNAME], dhcphdr->sname,
			    sizeof ( dhcphdr->sname ) );
	
	/* Set DHCP_OPTION_OVERLOAD option within the main options block */
	if ( set_dhcp_option ( &dhcppkt->options[OPTS_MAIN],
			       DHCP_OPTION_OVERLOAD, &overloading,
			       sizeof ( overloading ) ) == NULL )
		return -ENOSPC;

	/* Set DHCP_MESSAGE_TYPE option */
	if ( ( rc = set_dhcp_packet_option ( dhcppkt, DHCP_MESSAGE_TYPE,
					     &msgtype,
					     sizeof ( msgtype ) ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Calculate used length of a field containing DHCP options
 *
 * @v data		Field containing DHCP options
 * @v max_len		Field length
 * @ret len		Used length (excluding the @c DHCP_END tag)
 */
static size_t dhcp_field_len ( const void *data, size_t max_len ) {
	struct dhcp_option_block options;
	struct dhcp_option *end;

	options.data = ( ( void * ) data );
	options.len = max_len;
	end = find_dhcp_option ( &options, DHCP_END );
	return ( end ? ( ( ( void * ) end ) - data ) : 0 );
}

/**
 * Merge field containing DHCP options or string into DHCP options block
 *
 * @v options		DHCP option block
 * @v data		Field containing DHCP options
 * @v max_len		Field length
 * @v tag		DHCP option tag, or 0
 *
 * If @c tag is non-zero, the field will be treated as a
 * NUL-terminated string representing the value of the specified DHCP
 * option.  If @c tag is zero, the field will be treated as a block of
 * DHCP options, and simply appended to the existing options in the
 * option block.
 *
 * The caller must ensure that there is enough space in the options
 * block to perform the merge.
 */
static void merge_dhcp_field ( struct dhcp_option_block *options,
			       const void *data, size_t max_len,
			       unsigned int tag ) {
	size_t len;
	void *dest;
	struct dhcp_option *end;

	if ( tag ) {
		set_dhcp_option ( options, tag, data, strlen ( data ) );
	} else {
		len = dhcp_field_len ( data, max_len );
		dest = ( options->data + options->len - 1 );
		memcpy ( dest, data, len );
		options->len += len;
		end = ( dest + len );
		end->tag = DHCP_END;
	}
}

/**
 * Parse DHCP packet and construct DHCP options block
 *
 * @v dhcphdr		DHCP packet
 * @v len		Length of DHCP packet
 * @ret options		DHCP options block, or NULL
 *
 * Parses a received DHCP packet and canonicalises its contents into a
 * single DHCP options block.  The "file" and "sname" fields are
 * converted into the corresponding DHCP options (@c
 * DHCP_BOOTFILE_NAME and @c DHCP_TFTP_SERVER_NAME respectively).  If
 * these fields are used for option overloading, their options are
 * merged in to the options block.
 *
 * The values of the "yiaddr" and "siaddr" fields will be stored
 * within the options block as the magic options @c DHCP_EB_YIADDR and
 * @c DHCP_EB_SIADDR.
 * 
 * Note that this call allocates new memory for the constructed DHCP
 * options block; it is the responsibility of the caller to eventually
 * free this memory.
 */
static struct dhcp_option_block * dhcp_parse ( const struct dhcphdr *dhcphdr,
					       size_t len ) {
	struct dhcp_option_block *options;
	size_t options_len;
	unsigned int overloading;

	/* Sanity check */
	if ( len < sizeof ( *dhcphdr ) )
		return NULL;

	/* Calculate size of resulting concatenated option block:
	 *
	 *   The "options" field : length of the field minus the DHCP_END tag.
	 *
	 *   The "file" field : maximum length of the field minus the
	 *   NUL terminator, plus a 2-byte DHCP header or, if used for
	 *   option overloading, the length of the field minus the
	 *   DHCP_END tag.
	 *
	 *   The "sname" field : as for the "file" field.
	 *
	 *   15 bytes for an encapsulated options field to contain the
	 *   value of the "yiaddr" and "siaddr" fields
	 *
	 *   1 byte for a final terminating DHCP_END tag.
	 */
	options_len = ( ( len - offsetof ( typeof ( *dhcphdr ), options ) ) - 1
			+ ( sizeof ( dhcphdr->file ) + 1 )
			+ ( sizeof ( dhcphdr->sname ) + 1 )
			+ 15 /* yiaddr and siaddr */
			+ 1 /* DHCP_END tag */ );
	
	/* Allocate empty options block of required size */
	options = alloc_dhcp_options ( options_len );
	if ( ! options ) {
		DBG ( "DHCP could not allocate %d-byte option block\n",
		      options_len );
		return NULL;
	}
	
	/* Merge in "options" field, if this is a DHCP packet */
	if ( dhcphdr->magic == htonl ( DHCP_MAGIC_COOKIE ) ) {
		merge_dhcp_field ( options, dhcphdr->options,
				   ( len -
				     offsetof ( typeof (*dhcphdr), options ) ),
				   0 /* Always contains options */ );
	}

	/* Identify overloaded fields */
	overloading = find_dhcp_num_option ( options, DHCP_OPTION_OVERLOAD );
	
	/* Merge in "file" and "sname" fields */
	merge_dhcp_field ( options, dhcphdr->file, sizeof ( dhcphdr->file ),
			   ( ( overloading & DHCP_OPTION_OVERLOAD_FILE ) ?
			     0 : DHCP_BOOTFILE_NAME ) );
	merge_dhcp_field ( options, dhcphdr->sname, sizeof ( dhcphdr->sname ),
			   ( ( overloading & DHCP_OPTION_OVERLOAD_SNAME ) ?
			     0 : DHCP_TFTP_SERVER_NAME ) );

	/* Set magic options for "yiaddr" and "siaddr", if present */
	if ( dhcphdr->yiaddr.s_addr ) {
		set_dhcp_option ( options, DHCP_EB_YIADDR,
				  &dhcphdr->yiaddr, sizeof (dhcphdr->yiaddr) );
	}
	if ( dhcphdr->siaddr.s_addr ) {
		set_dhcp_option ( options, DHCP_EB_SIADDR,
				  &dhcphdr->siaddr, sizeof (dhcphdr->siaddr) );
	}
	
	assert ( options->len <= options->max_len );

	return options;
}

/****************************************************************************
 *
 * DHCP to UDP interface
 *
 */

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
	/** Option block registration routine */
	int ( * register_options ) ( struct net_device *netdev,
				     struct dhcp_option_block *options );

	/** State of the session
	 *
	 * This is a value for the @c DHCP_MESSAGE_TYPE option
	 * (e.g. @c DHCPDISCOVER).
	 */
	int state;
	/** Options obtained from server */
	struct dhcp_option_block *options;
	/** Retransmission timer */
	struct retry_timer timer;
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
	dhcpopt_put ( dhcp->options );
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
 * Transmit DHCP request
 *
 * @v dhcp		DHCP session
 * @ret rc		Return status code
 */
static int dhcp_send_request ( struct dhcp_session *dhcp ) {
	struct xfer_metadata meta = {
		.netdev = dhcp->netdev,
	};
	struct dhcp_packet dhcppkt;
	struct io_buffer *iobuf;
	int rc;
	
	DBGC ( dhcp, "DHCP %p transmitting %s\n",
	       dhcp, dhcp_msgtype_name ( dhcp->state ) );

	assert ( ( dhcp->state == DHCPDISCOVER ) ||
		 ( dhcp->state == DHCPREQUEST ) );

	/* Start retry timer.  Do this first so that failures to
	 * transmit will be retried.
	 */
	start_timer ( &dhcp->timer );

	/* Allocate buffer for packet */
	iobuf = xfer_alloc_iob ( &dhcp->xfer, DHCP_MIN_LEN );
	if ( ! iobuf )
		return -ENOMEM;

	/* Create DHCP packet in temporary buffer */
	if ( ( rc = create_dhcp_packet ( dhcp->netdev, dhcp->state,
					 iobuf->data, iob_tailroom ( iobuf ),
					 &dhcppkt ) ) != 0 ) {
		DBGC ( dhcp, "DHCP %p could not create DHCP packet: %s\n",
		       dhcp, strerror ( rc ) );
		goto done;
	}

	/* Copy in options common to all requests */
	if ( ( rc = copy_dhcp_packet_options ( &dhcppkt,
					       &dhcp_request_options ) ) != 0){
		DBGC ( dhcp, "DHCP %p could not set common DHCP options: %s\n",
		       dhcp, strerror ( rc ) );
		goto done;
	}

	/* Copy any required options from previous server repsonse */
	if ( dhcp->options ) {
		if ( ( rc = copy_dhcp_packet_option ( &dhcppkt, dhcp->options,
					    DHCP_SERVER_IDENTIFIER,
					    DHCP_SERVER_IDENTIFIER ) ) != 0 ) {
			DBGC ( dhcp, "DHCP %p could not set server identifier "
			       "option: %s\n", dhcp, strerror ( rc ) );
			goto done;
		}
		if ( ( rc = copy_dhcp_packet_option ( &dhcppkt, dhcp->options,
					    DHCP_EB_YIADDR,
					    DHCP_REQUESTED_ADDRESS ) ) != 0 ) {
			DBGC ( dhcp, "DHCP %p could not set requested address "
			       "option: %s\n", dhcp, strerror ( rc ) );
			goto done;
		}
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
 * Handle DHCP retry timer expiry
 *
 * @v timer		DHCP retry timer
 * @v fail		Failure indicator
 */
static void dhcp_timer_expired ( struct retry_timer *timer, int fail ) {
	struct dhcp_session *dhcp =
		container_of ( timer, struct dhcp_session, timer );

	if ( fail ) {
		dhcp_finished ( dhcp, -ETIMEDOUT );
	} else {
		dhcp_send_request ( dhcp );
	}
}

/**
 * Receive new data
 *
 * @v xfer 		Data transfer interface
 * @v iobuf		I/O buffer
 * @v data		Received data
 * @v len		Length of received data
 * @ret rc		Return status code
 */
static int dhcp_deliver_raw ( struct xfer_interface *xfer,
			      const void *data, size_t len ) {
	struct dhcp_session *dhcp =
		container_of ( xfer, struct dhcp_session, xfer );
	const struct dhcphdr *dhcphdr = data;
	struct dhcp_option_block *options;
	unsigned int msgtype;

	/* Check for matching transaction ID */
	if ( dhcphdr->xid != dhcp_xid ( dhcp->netdev ) ) {
		DBGC ( dhcp, "DHCP %p wrong transaction ID (wanted %08lx, "
			"got %08lx)\n", dhcp, ntohl ( dhcphdr->xid ),
			ntohl ( dhcp_xid ( dhcp->netdev ) ) );
		return 0;
	};

	/* Parse packet and create options structure */
	options = dhcp_parse ( dhcphdr, len );
	if ( ! options ) {
		DBGC ( dhcp, "DHCP %p could not parse DHCP packet\n", dhcp );
		return -EINVAL;
	}

	/* Determine message type */
	msgtype = find_dhcp_num_option ( options, DHCP_MESSAGE_TYPE );
	DBGC ( dhcp, "DHCP %p received %s\n",
	       dhcp, dhcp_msgtype_name ( msgtype ) );

	/* Handle DHCP reply */
	switch ( dhcp->state ) {
	case DHCPDISCOVER:
		if ( msgtype != DHCPOFFER )
			goto out_discard;
		dhcp->state = DHCPREQUEST;
		break;
	case DHCPREQUEST:
		if ( msgtype != DHCPACK )
			goto out_discard;
		dhcp->state = DHCPACK;
		break;
	default:
		assert ( 0 );
		goto out_discard;
	}

	/* Stop timer and update stored options */
	stop_timer ( &dhcp->timer );
	if ( dhcp->options )
		dhcpopt_put ( dhcp->options );
	dhcp->options = options;

	/* Transmit next packet, or terminate session */
	if ( dhcp->state < DHCPACK ) {
		dhcp_send_request ( dhcp );
	} else {
		dhcp->register_options ( dhcp->netdev, dhcp->options );
		dhcp_finished ( dhcp, 0 );
	}
	return 0;

 out_discard:
	dhcpopt_put ( options );
	return 0;
}

/** DHCP data transfer interface operations */
static struct xfer_interface_operations dhcp_xfer_operations = {
	.close		= ignore_xfer_close,
	.vredirect	= xfer_vopen,
	.request	= ignore_xfer_request,
	.seek		= ignore_xfer_seek,
	.deliver_iob	= xfer_deliver_as_raw,
	.deliver_raw	= dhcp_deliver_raw,
};

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
int start_dhcp ( struct job_interface *job, struct net_device *netdev,
		 int ( * register_options ) ( struct net_device *netdev,
					      struct dhcp_option_block * ) ) {
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
	dhcp = malloc ( sizeof ( *dhcp ) );
	if ( ! dhcp )
		return -ENOMEM;
	memset ( dhcp, 0, sizeof ( *dhcp ) );
	dhcp->refcnt.free = dhcp_free;
	job_init ( &dhcp->job, &dhcp_job_operations, &dhcp->refcnt );
	xfer_init ( &dhcp->xfer, &dhcp_xfer_operations, &dhcp->refcnt );
	dhcp->netdev = netdev_get ( netdev );
	dhcp->register_options = register_options;
	dhcp->timer.expired = dhcp_timer_expired;
	dhcp->state = DHCPDISCOVER;

	/* Instantiate child objects and attach to our interfaces */
	if ( ( rc = xfer_open_socket ( &dhcp->xfer, SOCK_DGRAM,
				       ( struct sockaddr * ) &server,
				       ( struct sockaddr * ) &client ) ) != 0 )
		goto err;

	/* Start timer to initiate initial DHCPREQUEST */
	start_timer ( &dhcp->timer );

	/* Attach parent interface, mortalise self, and return */
	job_plug_plug ( &dhcp->job, job );
	ref_put ( &dhcp->refcnt );
	return 0;

 err:
	dhcp_finished ( dhcp, rc );
	ref_put ( &dhcp->refcnt );
	return rc;
}

/****************************************************************************
 *
 * Network device configurator
 *
 */

/* Avoid dragging in dns.o */
struct sockaddr_tcpip nameserver;

/* Avoid dragging in syslog.o */
struct in_addr syslogserver;

/**
 * Configure network device from DHCP options
 *
 * @v netdev		Network device
 * @v options		DHCP options block
 * @ret rc		Return status code
 */
int dhcp_configure_netdev ( struct net_device *netdev,
			    struct dhcp_option_block *options ) {
	struct in_addr address = { 0 };
	struct in_addr netmask = { 0 };
	struct in_addr gateway = { INADDR_NONE };
	struct sockaddr_in *sin_nameserver;
	int rc;

	/* Clear any existing routing table entry */
	del_ipv4_address ( netdev );

	/* Retrieve IP address configuration */
	find_dhcp_ipv4_option ( options, DHCP_EB_YIADDR, &address );
	find_dhcp_ipv4_option ( options, DHCP_SUBNET_MASK, &netmask );
	find_dhcp_ipv4_option ( options, DHCP_ROUTERS, &gateway );

	/* Set up new IP address configuration */
	if ( ( rc = add_ipv4_address ( netdev, address, netmask,
				       gateway ) ) != 0 ) {
		DBG ( "Could not configure %s with DHCP results: %s\n",
		      netdev->name, strerror ( rc ) );
		return rc;
	}

	/* Retrieve other DHCP options that we care about */
	sin_nameserver = ( struct sockaddr_in * ) &nameserver;
	sin_nameserver->sin_family = AF_INET;
	find_dhcp_ipv4_option ( options, DHCP_DNS_SERVERS,
				&sin_nameserver->sin_addr );
	find_dhcp_ipv4_option ( options, DHCP_LOG_SERVERS,
				&syslogserver );

	return 0;
}
