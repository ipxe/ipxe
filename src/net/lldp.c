/*
 * Copyright (C) 2014 Marin Hannache <ipxe@mareo.fr>.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/if_ether.h>
#include <ipxe/if_arp.h>
#include <ipxe/iobuf.h>
#include <ipxe/netdevice.h>
#include <ipxe/lldp.h>

/** @file
 *
 * Link Layer Discovery Protocol
 *
 * This file implements the link layer discovery protocol as defined in
 * IEEE Std 802.1AB-2009.
 */

struct net_protocol lldp_protocol __net_protocol;

/****************************************************************************
 *
 * LLDP packet settings interface
 *
 * The LLDP setting space can be accessed both by using the predefined named
 * setting bellow, or directly with the LLDP TLV type.
 *
 * General syntax is: netX.lldp/<offset>.<size>.<count>.<type>
 * The fetched value being the <count>-th TLV of type <type>. <offset> and
 * <size> allow the user to get specific part of the answer in order to parse
 * TLV type unknown to iPXE, a <size> of zero means "until the end".
 *
 * Examples:
 *   iPXE> show netX.lldp/2.4.0.8:ipv4
 *   net0.lldp/2.4.0.8:ipv4 = 172.16.0.1
 *
 *   iPXE> show netX.lldp/4 # equivalent to: show netX.lldp/0.0.0.4
 *   net0.lldp/portdesc:string = tap0
 *
 *   iPXE> show netX.lldp/portid # equivalent to: show netX.lldp/0.0.0.2
 *   net0.lldp/portid:hex = 46:75:7a:43:c2:2f
 *
 */

/** LLDP setting scopes */
const struct settings_scope lldp_settings_scope;
const struct settings_scope lldp_special_scope;

/* Chassis ID and Port ID have a type depending of the TLV value */
const struct setting_type lldp_type_unknown = {
	.name = "unknown",
};

const struct setting lldp_lldpsource_setting __setting ( SETTING_LLDP,
                                                         000_lldpsource ) = {
	.name = "lldpsource",
	.description = "LLDP packet source address",
	.type = &setting_type_hex,
	.scope = &lldp_special_scope,
	.tag = 0x00000000,
};

const struct setting lldp_lldpdest_setting __setting ( SETTING_LLDP,
                                                       000_lldpdest ) = {
	.name = "lldpdest",
	.description = "LLDP packet destination address",
	.type = &setting_type_hex,
	.scope = &lldp_special_scope,
	.tag = 0x00000100,
};

const struct setting lldp_chassisid_setting __setting ( SETTING_LLDP,
                                                        001_chassisid ) = {
	.name = "chassisid",
	.description = "Chassis ID",
	.type = &lldp_type_unknown,
	.scope = &lldp_settings_scope,
	.tag = 0x00000001,
};

const struct setting lldp_portid_setting __setting ( SETTING_LLDP,
                                                     002_portid ) = {
	.name = "portid",
	.description = "Port ID",
	.type = &lldp_type_unknown,
	.scope = &lldp_settings_scope,
	.tag = 0x00000002,
};

const struct setting lldp_portdesc_setting __setting ( SETTING_LLDP,
                                                       004_portdesc ) = {
	.name = "portdesc",
	.description = "Port description",
	.type = &setting_type_string,
	.scope = &lldp_settings_scope,
	.tag = 0x00000004,
};

const struct setting lldp_systemname_setting __setting ( SETTING_LLDP,
                                                         005_systemname ) = {
	.name = "systemname",
	.description = "System name",
	.type = &setting_type_string,
	.scope = &lldp_settings_scope,
	.tag = 0x00000005,
};

const struct setting lldp_systemdesc_setting __setting ( SETTING_LLDP,
                                                         006_systemdesc ) = {
	.name = "systemdesc",
	.description = "System description",
	.type = &setting_type_string,
	.scope = &lldp_settings_scope,
	.tag = 0x00000006,
};

/**
 * Get the setting type of lldp_type_unknown settings.
 *
 * @v data		TLV value
 * @v type		TLV type
 * @v size		TLV length
 * @v skip		Number of byte to skip when using the data
 * @ret setting_type	Setting type to use when formating TLV value
 */
static const struct setting_type *get_setting_type ( const char *data,
                                                     uint8_t type,
                                                     uint16_t size,
                                                     uint16_t *skip ) {

	/* The LLDP specification describe a mechanism to know in which format
	 * data are encoded for TLV type 0x01 and 0x02 (Chassis ID and Port
	 * ID). This is a basicaly a one or two bytes header added before the
	 * actual data. You thought the LLDP protocol was so simple it was
	 * impossible to get it wrong? Don't underestimate the IEEE, after the
	 * 7 bits long TLV type, let me introduce the inconsistent format
	 * header values.
	 *
	 * type == 0x01:
	 *   - 0x01 || 0x02 || 0x03 || 0x06 || 0x07 -> string
	 *   - 0x04 -> hex
	 *   - 0x0501 -> ipv4
	 *   - 0x0502 -> ipv6
	 * type == 0x02:
	 *   - 0x01 || 0x02 || 0x05 || 0x06 || 0x07 -> string
	 *   - 0x03 -> hex
	 *   - 0x0401 -> ipv4
	 *   - 0x0402 -> ipv6
	 */
	if ( type <= 0x02 ) {
		if ( size < 1 )
			goto err_size;

		*skip = 1;

		/* type == 0x01: 0x06 - 0x01 == 0x05
		 * type == 0x02: 0x06 - 0x02 == 0x04 */
		if ( data[0] == 0x06 - type ) {
			if ( size < 2 )
				goto err_size;

			*skip = 2;
			if ( data[1] == 0x01 )
				return &setting_type_ipv4;

			if ( data[1] == 0x02 )
				return &setting_type_ipv6;
		}

		switch ( data[0] ) {
			case 0x01: case 0x02:
			case 0x06: case 0x07:
				return &setting_type_string;
		}

		/* type == 0x01: 0x01 + 2 * 0x01 == 0x03
		 * type == 0x02: 0x01 + 2 * 0x02 == 0x05 */
		if ( data[0] == 0x01 + 2 * type )
			return &setting_type_string;

		/* type == 0x01: 0x05 - 0x01 == 0x04
		 * type == 0x02: 0x05 - 0x02 == 0x03 */
		if ( data[0] == 0x05 - type )
			return &setting_type_hex;
	}

	/* No match found */

err_size:
	*skip = 0;
	return &setting_type_hex;
}



/**
 * Check applicability of LLDP setting
 *
 * @v settings		Settings block
 * @v setting		Setting
 * @ret applies		Setting applies within this settings block
 */
static int lldp_settings_applies ( struct settings *settings __unused,
                                   const struct setting *setting ) {
	return setting->scope == &lldp_settings_scope ||
	       setting->scope == &lldp_special_scope;
}

/**
 * Fetch value of LLDP setting
 *
 * @v settings		Settings block, or NULL to search all blocks
 * @v setting		Setting to fetch
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int lldp_settings_fetch ( struct settings *settings,
                                 struct setting *setting,
                                 void *buf,
                                 size_t len ) {
	struct lldp_settings *lldp_settings =
		container_of ( settings, struct lldp_settings, settings );
	uint8_t type;
	uint8_t count;
	uint16_t size, req_size;
	uint16_t offset;
	char *data;

	if ( LLDP_TAG_TYPE ( setting->tag ) == 0 &&
	     setting->scope != &lldp_special_scope )
		return -ENOENT;

	count = 0;

	/* First TLV is always valid since we created it ourselves */
	data  = lldp_parse ( lldp_settings->data, &type, &size );
	while ( data + size + 2 < lldp_settings->data + lldp_settings->size &&
		( type != 0 || size != 0 ) ) {
		if ( LLDP_TAG_TYPE ( setting->tag ) == type &&
		     LLDP_TAG_COUNT ( setting->tag ) == count++ ) {
			if ( setting->type == &lldp_type_unknown ) {
				setting->type = get_setting_type ( data, type,
				                                  size,
				                                  &offset );
				setting->tag = LLDP_TAG_SET_OFS ( setting->tag,
				                                  offset );
			}

			offset   = LLDP_TAG_OFS ( setting->tag );
			req_size = LLDP_TAG_LEN ( setting->tag );

			if ( offset >= size )
				return -EINVAL;

			if ( offset + req_size > size || req_size == 0 )
				req_size = size - offset;

			if ( len >= req_size )
				memcpy ( buf, data + offset, req_size );

			return req_size;
		}

		data = lldp_parse ( data + size, &type, &size );
	}

	return -ENOENT;
}


/** LLDP settings operations */
static struct settings_operations lldp_settings_operations = {
	.applies = lldp_settings_applies,
	.fetch = lldp_settings_fetch,
};

/****************************************************************************/

/**
 * Process incoming LLDP packets
 *
 * @v iobuf		I/O buffer
 * @v netdev		Network device
 * @v ll_source		Link-layer source address
 * @v flags		Packet flags
 * @ret rc		Return status code
 */
static int lldp_rx ( struct io_buffer *iobuf, struct net_device *netdev,
                     const void *ll_dest, const void *ll_source,
                     unsigned int flags __unused ) {
	struct lldp_settings *lldp_settings;
	struct ll_protocol *ll_protocol = netdev->ll_protocol;
	uint16_t *tlv_header;
	int rc = 0;

	lldp_settings = zalloc ( sizeof ( struct lldp_settings ) +
	                         2 * ( 2 + ll_protocol->ll_addr_len ) +
	                         iob_len ( iobuf ) );

	if (!lldp_settings) {
		rc = -ENOMEM;
		goto done;
	}

	ref_init ( &lldp_settings->refcnt, NULL );

	lldp_settings->size = 2 * ( 2 + ll_protocol->ll_addr_len ) +
	                      iob_len ( iobuf );

	/* Add a fake TLV with type 0 for source address */
	tlv_header = (uint16_t *) lldp_settings->data;
	*tlv_header = htons ( ll_protocol->ll_addr_len );
	memcpy ( tlv_header + 1, ll_source, ll_protocol->ll_addr_len );

	/* Add a fake TLV with type 0 for destination address */
	tlv_header = (uint16_t *) ( lldp_settings->data +
                                    2 + ll_protocol->ll_addr_len );
	*tlv_header = htons ( ll_protocol->ll_addr_len );
	memcpy ( tlv_header + 1, ll_dest, ll_protocol->ll_addr_len );

	/* Copy packet content */
	memcpy ( lldp_settings->data + 2 * ( 2 + ll_protocol->ll_addr_len ),
	         iobuf->data, iob_len ( iobuf ) );

	settings_init ( &lldp_settings->settings, &lldp_settings_operations,
	                &lldp_settings->refcnt, &lldp_settings_scope );

	register_settings ( &lldp_settings->settings,
	                    netdev_settings ( netdev ), LLDP_SETTINGS_NAME );

	ref_put ( &lldp_settings->refcnt );

done:
	free_iob ( iobuf );
	return rc;
}

/**
 * Transcribe LLDP address
 *
 * @v net_addr	(unused)
 * @ret string	"<LLDP>"
 *
 * This operation is meaningless for the LLDP protocol.
 */
static const char * lldp_ntoa ( const void *net_addr __unused ) {
	return "<LLDP>";
}

/** LLDP network protocol */
struct net_protocol lldp_protocol __net_protocol = {
	.name      = "LLDP",
	.net_proto = htons ( ETH_P_LLDP ),
	.rx        = lldp_rx,
	.ntoa      = lldp_ntoa,
};
