/*
 * Copyright (C) 2023 Michael Brown <mbrown@fensystems.co.uk>.
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
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * Link Layer Discovery Protocol
 *
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/iobuf.h>
#include <ipxe/netdevice.h>
#include <ipxe/if_ether.h>
#include <ipxe/settings.h>
#include <ipxe/lldp.h>

/** An LLDP settings block */
struct lldp_settings {
	/** Settings interface */
	struct settings settings;
	/** Name */
	const char *name;
	/** LLDP data */
	void *data;
	/** Length of LLDP data */
	size_t len;
};

/* Forward declaration */
struct net_driver lldp_driver __net_driver;

/** LLDP settings scope */
static const struct settings_scope lldp_settings_scope;

/**
 * Check applicability of LLDP setting
 *
 * @v settings		Settings block
 * @v setting		Setting to fetch
 * @ret applies		Setting applies within this settings block
 */
static int lldp_applies ( struct settings *settings __unused,
			  const struct setting *setting ) {

	return ( setting->scope == &lldp_settings_scope );
}

/**
 * Fetch value of LLDP setting
 *
 * @v settings		Settings block
 * @v setting		Setting to fetch
 * @v buf		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int lldp_fetch ( struct settings *settings,
			struct setting *setting,
			void *buf, size_t len ) {
	struct lldp_settings *lldpset =
		container_of ( settings, struct lldp_settings, settings );
	union {
		uint32_t high;
		uint8_t raw[4];
	} tag_prefix;
	uint32_t tag_low;
	uint8_t tag_type;
	uint8_t tag_index;
	uint8_t tag_offset;
	uint8_t tag_length;
	const void *match;
	const void *data;
	size_t match_len;
	size_t remaining;
	const struct lldp_tlv *tlv;
	unsigned int tlv_type_len;
	unsigned int tlv_type;
	unsigned int tlv_len;

	/* Parse setting tag */
	tag_prefix.high = htonl ( setting->tag >> 32 );
	tag_low = setting->tag;
	tag_type = ( tag_low >> 24 );
	tag_index = ( tag_low >> 16 );
	tag_offset = ( tag_low >> 8 );
	tag_length = ( tag_low >> 0 );

	/* Identify match prefix */
	match_len = tag_offset;
	if ( match_len > sizeof ( tag_prefix ) )
		match_len = sizeof ( tag_prefix );
	if ( ! tag_prefix.high )
		match_len = 0;
	match = &tag_prefix.raw[ sizeof ( tag_prefix ) - match_len ];

	/* Locate matching TLV */
	for ( data = lldpset->data, remaining = lldpset->len ; remaining ;
	      data += tlv_len, remaining -= tlv_len ) {

		/* Parse TLV header */
		if ( remaining < sizeof ( *tlv ) ) {
			DBGC ( lldpset, "LLDP %s underlength TLV header\n",
			       lldpset->name );
			DBGC_HDA ( lldpset, 0, data, remaining );
			break;
		}
		tlv = data;
		data += sizeof ( *tlv );
		remaining -= sizeof ( *tlv );
		tlv_type_len = ntohs ( tlv->type_len );
		tlv_type = LLDP_TLV_TYPE ( tlv_type_len );
		if ( tlv_type == LLDP_TYPE_END )
			break;
		tlv_len = LLDP_TLV_LEN ( tlv_type_len );
		if ( remaining < tlv_len ) {
			DBGC ( lldpset, "LLDP %s underlength TLV value\n",
			       lldpset->name );
			DBGC_HDA ( lldpset, 0, data, remaining );
			break;
		}
		DBGC2 ( lldpset, "LLDP %s found type %d:\n",
			lldpset->name, tlv_type );
		DBGC2_HDA ( lldpset, 0, data, tlv_len );

		/* Check for matching tag type */
		if ( tlv_type != tag_type )
			continue;

		/* Check for matching prefix */
		if ( tlv_len < match_len )
			continue;
		if ( memcmp ( data, match, match_len ) != 0 )
			continue;

		/* Check for matching index */
		if ( tag_index-- )
			continue;

		/* Skip offset */
		if ( tlv_len < tag_offset )
			return 0;
		data += tag_offset;
		tlv_len -= tag_offset;

		/* Set type if not already specified */
		if ( ! setting->type ) {
			setting->type = ( tag_length ? &setting_type_hex :
					  &setting_type_string );
		}

		/* Extract value */
		if ( tag_length && ( tlv_len > tag_length ) )
			tlv_len = tag_length;
		if ( len > tlv_len )
			len = tlv_len;
		memcpy ( buf, data, len );
		return tlv_len;
	}

	return -ENOENT;
}

/** LLDP settings operations */
static struct settings_operations lldp_settings_operations = {
	.applies = lldp_applies,
	.fetch = lldp_fetch,
};

/**
 * Process LLDP packet
 *
 * @v iobuf		I/O buffer
 * @v netdev		Network device
 * @v ll_dest		Link-layer destination address
 * @v ll_source		Link-layer source address
 * @v flags		Packet flags
 * @ret rc		Return status code
 */
static int lldp_rx ( struct io_buffer *iobuf, struct net_device *netdev,
		     const void *ll_dest, const void *ll_source,
		     unsigned int flags __unused ) {
	struct lldp_settings *lldpset;
	size_t len;
	void *data;
	int rc;

	/* Find matching LLDP settings block */
	lldpset = netdev_priv ( netdev, &lldp_driver );

	/* Create trimmed copy of received LLDP data */
	len = iob_len ( iobuf );
	data = malloc ( len );
	if ( ! data ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	memcpy ( data, iobuf->data, len );

	/* Free any existing LLDP data */
	free ( lldpset->data );

	/* Transfer data to LLDP settings block */
	lldpset->data = data;
	lldpset->len = len;
	data = NULL;
	DBGC2 ( lldpset, "LLDP %s src %s ",
		lldpset->name, netdev->ll_protocol->ntoa ( ll_source ) );
	DBGC2 ( lldpset, "dst %s\n", netdev->ll_protocol->ntoa ( ll_dest ) );
	DBGC2_HDA ( lldpset, 0, lldpset->data, lldpset->len );

	/* Success */
	rc = 0;

	free ( data );
 err_alloc:
	free_iob ( iobuf );
	return rc;
}

/** LLDP protocol */
struct net_protocol lldp_protocol __net_protocol = {
	.name = "LLDP",
	.net_proto = htons ( ETH_P_LLDP ),
	.rx = lldp_rx,
};

/**
 * Create LLDP settings block
 *
 * @v netdev		Network device
 * @v priv		Private data
 * @ret rc		Return status code
 */
static int lldp_probe ( struct net_device *netdev, void *priv ) {
	struct lldp_settings *lldpset = priv;
	int rc;

	/* Initialise LLDP settings block */
	settings_init ( &lldpset->settings, &lldp_settings_operations,
			&netdev->refcnt, &lldp_settings_scope );
	lldpset->name = netdev->name;

	/* Register settings */
	if ( ( rc = register_settings ( &lldpset->settings,
					netdev_settings ( netdev ),
					LLDP_SETTINGS_NAME ) ) != 0 ) {
		DBGC ( lldpset, "LLDP %s could not register settings: %s\n",
		       lldpset->name, strerror ( rc ) );
		goto err_register;
	}
	DBGC ( lldpset, "LLDP %s registered\n", lldpset->name );

	return 0;

	unregister_settings ( &lldpset->settings );
 err_register:
	assert ( lldpset->data == NULL );
	return rc;
}

/**
 * Remove LLDP settings block
 *
 * @v netdev		Network device
 * @v priv		Private data
 */
static void lldp_remove ( struct net_device *netdev __unused, void *priv ) {
	struct lldp_settings *lldpset = priv;

	/* Unregister settings */
	unregister_settings ( &lldpset->settings );
	DBGC ( lldpset, "LLDP %s unregistered\n", lldpset->name );

	/* Free any LLDP data */
	free ( lldpset->data );
	lldpset->data = NULL;
}

/** LLDP driver */
struct net_driver lldp_driver __net_driver = {
	.name = "LLDP",
	.priv_len = sizeof ( struct lldp_settings ),
	.probe = lldp_probe,
	.remove = lldp_remove,
};
