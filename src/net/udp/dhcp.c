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
#include <gpxe/netdevice.h>
#include <gpxe/dhcp.h>

/** @file
 *
 * Dynamic Host Configuration Protocol
 *
 */

struct dhcp_session {
	struct net_device *netdev;
	uint32_t xid;
};

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

/** DHCP packet option block fill order
 *
 * This is the order in which option blocks are filled when
 * reassembling a DHCP packet.  We fill the smallest field ("sname")
 * first, to maximise the chances of being able to fit large options
 * within fields which are large enough to contain them.
 */
enum dhcp_packet_option_block_fill_order {
	OPTS_SNAME = 0,
	OPTS_FILE,
	OPTS_MAIN,
	NUM_OPT_BLOCKS
};

/** DHCP option blocks within a DHCP packet
 *
 * A DHCP packet contains three fields which can be used to contain
 * options: the actual "options" field plus the "file" and "sname"
 * fields (which can be overloaded to contain options).
 */
struct dhcp_packet_option_blocks {
	struct dhcp_option_block options[NUM_OPT_BLOCKS];
};

/**
 * Set option within DHCP packet
 *
 * @v optblocks		DHCP packet option blocks
 * @v tag		DHCP option tag
 * @v data		New value for DHCP option
 * @v len		Length of value, in bytes
 * @ret option		DHCP option, or NULL
 *
 * Sets the option within the first available options block within the
 * DHCP packet.  Option blocks are tried in the order specified by @c
 * dhcp_option_block_fill_order.
 */
static struct dhcp_option *
set_dhcp_packet_option ( struct dhcp_packet_option_blocks *optblocks,
			 unsigned int tag, const void *data, size_t len ) {
	struct dhcp_option_block *options;
	struct dhcp_option *option;

	for ( options = optblocks->options ;
	      options < &optblocks->options[NUM_OPT_BLOCKS] ; options++ ) {
		option = set_dhcp_option ( options, tag, data, len );
		if ( option )
			return option;
	}
	return NULL;
}

/**
 * Copy options to DHCP packet
 *
 * @v optblocks		DHCP packet option blocks
 * @v encapsulator	Encapsulating option, or zero
 * @ret rc		Return status code
 * 
 * Copies options from DHCP options blocks into a DHCP packet.  Most
 * options are copied verbatim.  Recognised encapsulated options
 * fields are handled as such.  Selected options (e.g. @c
 * DHCP_OPTION_OVERLOAD) are always ignored, since these special cases
 * are handled by other code.
 */
static int
copy_dhcp_options_to_packet ( struct dhcp_packet_option_blocks *optblocks,
			      unsigned int encapsulator ) {
	unsigned int subtag;
	unsigned int tag;
	struct dhcp_option *option;
	struct dhcp_option *copied;
	int rc;

	for ( subtag = DHCP_MIN_OPTION; subtag <= DHCP_MAX_OPTION; subtag++ ) {
		tag = DHCP_ENCAP_OPT ( encapsulator, subtag );
		switch ( tag ) {
		case DHCP_OPTION_OVERLOAD:
			/* Hard-coded in packets we reassemble; skip
			 * this option
			 */
			break;
		case DHCP_EB_ENCAP:
		case DHCP_VENDOR_ENCAP:
			/* Process encapsulated options field */
			if ( ( rc = copy_dhcp_options_to_packet ( optblocks,
								  tag ) ) != 0)
				return rc;
			break;
		default:
			/* Copy option to reassembled packet */
			option = find_global_dhcp_option ( tag );
			if ( ! option )
				break;
			copied = set_dhcp_packet_option ( optblocks, tag,
							  &option->data,
							  option->len );
			if ( ! copied )
				return -ENOSPC;
			break;
		};
	}

	return 0;
}

/**
 * Assemble a DHCP packet
 *
 * @v dhcp		DHCP session
 * @v data		Packet to be filled in
 * @v max_len		Length of packet buffer
 * @ret len		Length of assembled packet
 *
 * Reconstruct a DHCP packet from a DHCP options list.
 */
size_t dhcp_assemble ( struct dhcp_session *dhcp, void *data,
		       size_t max_len ) {
	struct dhcp_packet *dhcppkt = data;
	struct dhcp_option *option;
	struct dhcp_packet_option_blocks optblocks;
	unsigned int dhcp_message_type;
	static const uint8_t overloading = ( DHCP_OPTION_OVERLOAD_FILE |
					     DHCP_OPTION_OVERLOAD_SNAME );

	/* Fill in constant fields */
	memset ( dhcppkt, 0, max_len );
	dhcppkt->xid = dhcp->xid;
	dhcppkt->magic = htonl ( DHCP_MAGIC_COOKIE );

	/* Derive "op" field from DHCP_MESSAGE_TYPE option value */
	dhcp_message_type = find_global_dhcp_num_option ( DHCP_MESSAGE_TYPE );
	dhcppkt->op = dhcp_op[dhcp_message_type];

	/* Fill in NIC details */
	dhcppkt->htype = dhcp->netdev->ll_protocol->ll_proto;
	dhcppkt->hlen = dhcp->netdev->ll_protocol->ll_addr_len;
	memcpy ( dhcppkt->chaddr, dhcp->netdev->ll_addr, dhcppkt->hlen );

	/* Fill in IP addresses if present */
	option = find_global_dhcp_option ( DHCP_EB_YIADDR );
	if ( option ) {
		memcpy ( &dhcppkt->yiaddr, &option->data,
			 sizeof ( dhcppkt->yiaddr ) );
	}
	option = find_global_dhcp_option ( DHCP_EB_SIADDR );
	if ( option ) {
		memcpy ( &dhcppkt->siaddr, &option->data,
			 sizeof ( dhcppkt->siaddr ) );
	}

	/* Initialise option blocks */
	init_dhcp_options ( &optblocks.options[OPTS_MAIN], dhcppkt->options,
			    ( max_len -
			      offsetof ( typeof ( *dhcppkt ), options ) ) );
	init_dhcp_options ( &optblocks.options[OPTS_FILE], dhcppkt->file,
			    sizeof ( dhcppkt->file ) );
	init_dhcp_options ( &optblocks.options[OPTS_SNAME], dhcppkt->sname,
			    sizeof ( dhcppkt->sname ) );
	set_dhcp_option ( &optblocks.options[OPTS_MAIN], DHCP_OPTION_OVERLOAD,
			  &overloading, sizeof ( overloading ) );

	/* Populate option blocks */
	copy_dhcp_options_to_packet ( &optblocks, 0 );

	return ( offsetof ( typeof ( *dhcppkt ), options )
		 + optblocks.options[OPTS_MAIN].len );
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
 * @v data		DHCP packet
 * @v len		Length of DHCP packet
 * @ret options		DHCP options block, or NULL
 *
 * Parses a received DHCP packet and canonicalises its contents into a
 * single DHCP options block.  The "file" and "sname" fields are
 * converted into the corresponding DHCP options (@c
 * DHCP_BOOTFILE_NAME and @c DHCP_TFTP_SERVER_NAME respectively).  If
 * these fields are used for option overloading, their options are
 * merged in to the options block.  The values of the "yiaddr" and
 * "siaddr" fields will be stored within the options block as the
 * options @c DHCP_EB_YIADDR and @c DHCP_EB_SIADDR.
 * 
 * Note that this call allocates new memory for the constructed DHCP
 * options block; it is the responsibility of the caller to eventually
 * free this memory.
 */
struct dhcp_option_block * dhcp_parse ( const void *data, size_t len ) {
	const struct dhcp_packet *dhcppkt = data;
	struct dhcp_option_block *options;
	size_t options_len;
	unsigned int overloading;

	/* Sanity check */
	if ( len < sizeof ( *dhcppkt ) )
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
	options_len = ( ( len - offsetof ( typeof ( *dhcppkt ), options ) ) - 1
			+ ( sizeof ( dhcppkt->file ) + 1 )
			+ ( sizeof ( dhcppkt->sname ) + 1 )
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
	if ( dhcppkt->magic == htonl ( DHCP_MAGIC_COOKIE ) ) {
		merge_dhcp_field ( options, dhcppkt->options,
				   ( len -
				     offsetof ( typeof (*dhcppkt), options ) ),
				   0 /* Always contains options */ );
	}

	/* Identify overloaded fields */
	overloading = find_dhcp_num_option ( options, DHCP_OPTION_OVERLOAD );
	
	/* Merge in "file" and "sname" fields */
	merge_dhcp_field ( options, dhcppkt->file, sizeof ( dhcppkt->file ),
			   ( ( overloading & DHCP_OPTION_OVERLOAD_FILE ) ?
			     DHCP_BOOTFILE_NAME : 0 ) );
	merge_dhcp_field ( options, dhcppkt->sname, sizeof ( dhcppkt->sname ),
			   ( ( overloading & DHCP_OPTION_OVERLOAD_SNAME ) ?
			     DHCP_TFTP_SERVER_NAME : 0 ) );

	/* Set options for "yiaddr" and "siaddr", if present */
	if ( dhcppkt->yiaddr.s_addr ) {
		set_dhcp_option ( options, DHCP_EB_YIADDR,
				  &dhcppkt->yiaddr, sizeof (dhcppkt->yiaddr) );
	}
	if ( dhcppkt->siaddr.s_addr ) {
		set_dhcp_option ( options, DHCP_EB_SIADDR,
				  &dhcppkt->siaddr, sizeof (dhcppkt->siaddr) );
	}
	
	assert ( options->len <= options->max_len );

	return options;
}
