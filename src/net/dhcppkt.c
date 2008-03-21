/*
 * Copyright (C) 2008 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <gpxe/netdevice.h>
#include <gpxe/dhcp.h>
#include <gpxe/dhcpopts.h>
#include <gpxe/dhcppkt.h>

/** @file
 *
 * DHCP packets
 *
 */

/** A dedicated field within a DHCP packet */
struct dhcp_packet_field {
	/** Settings tag number */
	unsigned int tag;
	/** Offset within DHCP packet */
	uint16_t offset;
	/** Length of field */
	uint16_t len;
};

/** Declare a dedicated field within a DHCP packet
 *
 * @v _tag		Settings tag number
 * @v _field		Field name
 */
#define DHCP_PACKET_FIELD( _tag, _field ) {				\
		.tag = (_tag),						\
		.offset = offsetof ( struct dhcphdr, _field ),		\
		.len = sizeof ( ( ( struct dhcphdr * ) 0 )->_field ),	\
	}

/** Dedicated fields within a DHCP packet */
static struct dhcp_packet_field dhcp_packet_fields[] = {
	DHCP_PACKET_FIELD ( DHCP_EB_YIADDR, yiaddr ),
	DHCP_PACKET_FIELD ( DHCP_EB_SIADDR, siaddr ),
	DHCP_PACKET_FIELD ( DHCP_TFTP_SERVER_NAME, sname ),
	DHCP_PACKET_FIELD ( DHCP_BOOTFILE_NAME, file ),
};

/**
 * Get address of a DHCP packet field
 *
 * @v dhcphdr		DHCP packet header
 * @v field		DHCP packet field
 * @ret data		Packet field data
 */
static inline void * dhcp_packet_field ( struct dhcphdr *dhcphdr,
					 struct dhcp_packet_field *field ) {
	return ( ( ( void * ) dhcphdr ) + field->offset );
}

/**
 * Find DHCP packet field corresponding to settings tag number
 *
 * @v tag		Settings tag number
 * @ret field		DHCP packet field, or NULL
 */
static struct dhcp_packet_field *
find_dhcp_packet_field ( unsigned int tag ) {
	struct dhcp_packet_field *field;
	unsigned int i;

	for ( i = 0 ; i < ( sizeof ( dhcp_packet_fields ) /
			    sizeof ( dhcp_packet_fields[0] ) ) ; i++ ) {
		field = &dhcp_packet_fields[i];
		if ( field->tag == tag )
			return field;
	}
	return NULL;
}
				    
/**
 * Store value of DHCP packet setting
 *
 * @v settings		Settings block
 * @v tag		Setting tag number
 * @v data		Setting data, or NULL to clear setting
 * @v len		Length of setting data
 * @ret rc		Return status code
 */
static int dhcppkt_store ( struct settings *settings, unsigned int tag,
			   const void *data, size_t len ) {
	struct dhcp_packet *dhcppkt =
		container_of ( settings, struct dhcp_packet, settings );
	struct dhcp_packet_field *field;
	int rc;

	/* If this is a special field, fill it in */
	if ( ( field = find_dhcp_packet_field ( tag ) ) != NULL ) {
		if ( len > field->len )
			return -ENOSPC;
		memcpy ( dhcp_packet_field ( dhcppkt->dhcphdr, field ),
			 data, len );
		return 0;
	}

	/* Otherwise, use the generic options block */
	rc = dhcpopt_store ( &dhcppkt->options, tag, data, len );

	/* Update our used-length field */
	dhcppkt->len = ( offsetof ( struct dhcphdr, options ) +
			 dhcppkt->options.len );

	return rc;
}

/**
 * Fetch value of DHCP packet setting
 *
 * @v settings		Settings block
 * @v tag		Setting tag number
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int dhcppkt_fetch ( struct settings *settings, unsigned int tag,
			   void *data, size_t len ) {
	struct dhcp_packet *dhcppkt =
		container_of ( settings, struct dhcp_packet, settings );
	struct dhcp_packet_field *field;
	
	/* If this is a special field, return it */
	if ( ( field = find_dhcp_packet_field ( tag ) ) != NULL ) {
		if ( len > field->len )
			len = field->len;
		memcpy ( data,
			 dhcp_packet_field ( dhcppkt->dhcphdr, field ), len );
		return field->len;
	}

	/* Otherwise, use the generic options block */
	return dhcpopt_fetch ( &dhcppkt->options, tag, data, len );
}

/** DHCP settings operations */
static struct settings_operations dhcppkt_settings_operations = {
	.store = dhcppkt_store,
	.fetch = dhcppkt_fetch,
};

/**
 * Initialise prepopulated DHCP packet
 *
 * @v dhcppkt		Uninitialised DHCP packet
 * @v refcnt		Reference counter of containing object, or NULL
 * @v data		Memory for DHCP packet data
 * @v max_len		Length of memory for DHCP packet data
 *
 * The memory content must already be filled with valid DHCP options.
 * A zeroed block counts as a block of valid DHCP options.
 */
void dhcppkt_init ( struct dhcp_packet *dhcppkt, struct refcnt *refcnt,
		    void *data, size_t len ) {
	dhcppkt->dhcphdr = data;
	dhcppkt->max_len = len;
	dhcpopt_init ( &dhcppkt->options, &dhcppkt->dhcphdr->options,
		       ( len - offsetof ( struct dhcphdr, options ) ) );
	dhcppkt->len = ( offsetof ( struct dhcphdr, options ) +
			 dhcppkt->options.len );
	settings_init ( &dhcppkt->settings, &dhcppkt_settings_operations,
			refcnt, "dhcp" );
}
