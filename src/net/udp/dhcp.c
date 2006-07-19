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
#include <errno.h>
#include <assert.h>
#include <byteswap.h>
#include <gpxe/if_ether.h>
#include <gpxe/netdevice.h>
#include <gpxe/udp.h>
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
	DHCP_OPTION ( DHCP_SUBNET_MASK, DHCP_ROUTERS, DHCP_HOST_NAME ),
	DHCP_END
};

/**
 * Name a DHCP packet type
 *
 * @v msgtype		DHCP message type
 * @ret string		DHCP mesasge type name
 */
static inline const char * dhcp_message_type_name ( unsigned int msgtype ) {
	switch ( msgtype ) {
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

/** Options common to all DHCP requests */
static struct dhcp_option_block dhcp_request_options = {
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
	struct dhcp_option_block *options;
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
	default:
		/* Continue processing as normal */
		break;
	}
		
	/* Set option in first available options block */
	for ( options = dhcppkt->options ;
	      options < &dhcppkt->options[NUM_OPT_BLOCKS] ; options++ ) {
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
 * Set options within DHCP packet
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
static int set_dhcp_packet_encap_options ( struct dhcp_packet *dhcppkt,
					   struct dhcp_option_block *options,
					   unsigned int encapsulator ) {
	unsigned int subtag;
	unsigned int tag;
	struct dhcp_option *option;
	int rc;

	for ( subtag = DHCP_MIN_OPTION; subtag <= DHCP_MAX_OPTION; subtag++ ) {
		tag = DHCP_ENCAP_OPT ( encapsulator, subtag );
		switch ( tag ) {
		case DHCP_EB_ENCAP:
		case DHCP_VENDOR_ENCAP:
			/* Process encapsulated options field */
			if ( ( rc = set_dhcp_packet_encap_options ( dhcppkt,
								    options,
								    tag )) !=0)
				return rc;
			break;
		default:
			/* Copy option to reassembled packet */
			option = find_dhcp_option ( options, tag );
			if ( ! option )
				break;
			if ( ( rc = set_dhcp_packet_option ( dhcppkt, tag,
							     &option->data,
							     option->len)) !=0)
				return rc;
			break;
		};
	}

	return 0;
}

/**
 * Set options within DHCP packet
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
static int set_dhcp_packet_options ( struct dhcp_packet *dhcppkt,
				     struct dhcp_option_block *options ) {
	return set_dhcp_packet_encap_options ( dhcppkt, options, 0 );
}

/**
 * Create a DHCP packet
 *
 * @v dhcp		DHCP session
 * @v msgtype		DHCP message type
 * @v data		Buffer for DHCP packet
 * @v max_len		Size of DHCP packet buffer
 * @v dhcppkt		DHCP packet structure to fill in
 * @ret rc		Return status code
 *
 * Creates a DHCP packet in the specified buffer, and fills out a @c
 * dhcp_packet structure that can be passed to
 * set_dhcp_packet_option() or set_dhcp_packet_options().
 */
static int create_dhcp_packet ( struct dhcp_session *dhcp, uint8_t msgtype,
				void *data, size_t max_len,
				struct dhcp_packet *dhcppkt ) {
	struct dhcphdr *dhcphdr = data;
	static const uint8_t overloading = ( DHCP_OPTION_OVERLOAD_FILE |
					     DHCP_OPTION_OVERLOAD_SNAME );
	int rc;

	/* Initialise DHCP packet content */
	memset ( dhcphdr, 0, max_len );
	dhcphdr->xid = dhcp->xid;
	dhcphdr->magic = htonl ( DHCP_MAGIC_COOKIE );
	dhcphdr->htype = ntohs ( dhcp->netdev->ll_protocol->ll_proto );
	dhcphdr->hlen = dhcp->netdev->ll_protocol->ll_addr_len;
	memcpy ( dhcphdr->chaddr, dhcp->netdev->ll_addr, dhcphdr->hlen );
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
static struct dhcp_option_block * dhcp_parse ( struct dhcphdr *dhcphdr,
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
			     DHCP_BOOTFILE_NAME : 0 ) );
	merge_dhcp_field ( options, dhcphdr->sname, sizeof ( dhcphdr->sname ),
			   ( ( overloading & DHCP_OPTION_OVERLOAD_SNAME ) ?
			     DHCP_TFTP_SERVER_NAME : 0 ) );

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

static inline struct dhcp_session *
udp_to_dhcp ( struct udp_connection *conn ) {
	return container_of ( conn, struct dhcp_session, udp );
}

/**
 * Transmit DHCP request
 *
 * @v conn		UDP connection
 * @v buf		Temporary data buffer
 * @v len		Length of temporary data buffer
 */
static void dhcp_senddata ( struct udp_connection *conn,
			    void *buf, size_t len ) {
	struct dhcp_session *dhcp = udp_to_dhcp ( conn );
	struct dhcp_packet dhcppkt;
	int rc;
	
	DBG ( "Transmitting %s\n", dhcp_message_type_name ( dhcp->state ) );

	assert ( ( dhcp->state == DHCPDISCOVER ) ||
		 ( dhcp->state == DHCPREQUEST ) );

	/* Create DHCP packet in temporary buffer */
	if ( ( rc = create_dhcp_packet ( dhcp, dhcp->state, buf, len,
					 &dhcppkt ) ) != 0 ) {
		DBG ( "Could not create DHCP packet\n" );
		return;
	}

	/* Copy in options common to all requests */
	if ( ( rc = set_dhcp_packet_options ( &dhcppkt,
					      &dhcp_request_options ) ) != 0 ){
		DBG ( "Could not set common DHCP options\n" );
		return;
	}

	/* Transmit the packet */
	udp_send ( conn, dhcppkt.dhcphdr, dhcppkt.len );
}

/**
 * Receive new data
 *
 * @v udp		UDP connection
 * @v data		Received data
 * @v len		Length of received data
 */
static void dhcp_newdata ( struct udp_connection *conn,
			   void *data, size_t len ) {
	struct dhcp_session *dhcp = udp_to_dhcp ( conn );
	struct dhcphdr *dhcphdr = data;
	struct dhcp_option_block *options;

	/* Check for matching transaction ID */
	if ( dhcphdr->xid != dhcp->xid ) {
		DBG ( "DHCP wrong transaction ID (wanted %08lx, got %08lx)\n",
		      ntohl ( dhcphdr->xid ), ntohl ( dhcp->xid ) );
		return;
	};

	/* Parse packet and create options structure */
	options = dhcp_parse ( dhcphdr, len );
	if ( ! options ) {
		DBG ( "Could not parse DHCP packet\n" );
		return;
	}

	DBG ( "Received %s\n",
	      dhcp_message_type_name ( find_dhcp_num_option ( options,
						       DHCP_MESSAGE_TYPE ) ) );

	/* Proof of concept: just dump out the parsed options */
	hex_dump ( options->data, options->len );
	free_dhcp_options ( options );
}

/** DHCP UDP operations */
static struct udp_operations dhcp_udp_operations = {
	.senddata	= dhcp_senddata,
	.newdata	= dhcp_newdata,
};

/**
 * Initiate DHCP on a network interface
 *
 * @v dhcp		DHCP session
 * @ret aop		Asynchronous operation
 */
struct async_operation * start_dhcp ( struct dhcp_session *dhcp ) {
	dhcp->udp.udp_op = &dhcp_udp_operations;
	dhcp->state = DHCPDISCOVER;
	/* Use least significant 32 bits of link-layer address as XID */
	memcpy ( &dhcp->xid, ( dhcp->netdev->ll_addr
			       + dhcp->netdev->ll_protocol->ll_addr_len
			       - sizeof ( dhcp->xid ) ), sizeof ( dhcp->xid ));

	/* Proof of concept: just send a single DHCPDISCOVER */
	udp_senddata ( &dhcp->udp );

	return &dhcp->aop;
}
