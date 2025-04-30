/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ipxe/usb.h>
#include <ipxe/settings.h>
#include <ipxe/init.h>

/** @file
 *
 * USB device settings
 *
 */

/** USB device settings scope */
static const struct settings_scope usb_settings_scope;

/**
 * Check applicability of USB device setting
 *
 * @v settings		Settings block
 * @v setting		Setting
 * @ret applies		Setting applies within this settings block
 */
static int usb_settings_applies ( struct settings *settings __unused,
				  const struct setting *setting ) {

	return ( setting->scope == &usb_settings_scope );
}

/**
 * Fetch value of USB device setting
 *
 * @v settings		Settings block
 * @v setting		Setting to fetch
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int usb_settings_fetch ( struct settings *settings __unused,
				struct setting *setting,
				void *data, size_t len ) {
	uint8_t *dst = data;
	const uint8_t *src;
	const uint8_t *desc;
	struct usb_bus *bus;
	struct usb_device *usb;
	int tag_direction;
	unsigned int tag_busdev;
	unsigned int tag_offset;
	unsigned int tag_len;
	unsigned int index;
	int rc;

	/* Extract parameters from tag */
	tag_direction = ( ( setting->tag & ( 1 << 31 ) ) ? +1 : -1 );
	tag_busdev = ( ( setting->tag >> 16 ) & 0x7fff );
	tag_offset = ( ( setting->tag >> 8 ) & 0xff );
	tag_len = ( ( setting->tag >> 0 ) & 0xff );

	/* Locate USB device */
	bus = find_usb_bus ( USB_BUS ( tag_busdev ) );
	if ( ! bus )
		return -ENODEV;
	usb = find_usb ( bus, USB_DEV ( tag_busdev ) );
	if ( ! usb )
		return -ENODEV;
	desc = ( ( const uint8_t * ) &usb->device );

	/* Following the usage of SMBIOS settings tags, a <length> of
	 * zero indicates that the byte at <offset> contains a string
	 * index.  An <offset> of zero indicates that the <length>
	 * contains a literal string index.
	 *
	 * Since the byte at offset zero can never contain a string
	 * index, and a literal string index can never be zero, the
	 * combination of both <length> and <offset> being zero
	 * indicates that the entire structure is to be read.
	 *
	 * By default we reverse the byte direction since USB values
	 * are little-endian and iPXE settings are big-endian.  We
	 * invert this default when reading the entire structure.
	 */
	if ( ( tag_len == 0 ) && ( tag_offset == 0 ) ) {
		tag_len = sizeof ( usb->device );
		tag_direction = -tag_direction;
	} else if ( ( tag_len == 0 ) || ( tag_offset == 0 ) ) {
		index = tag_len;
		if ( ( ! index ) && ( tag_offset < sizeof ( usb->device ) ) )
			index = desc[tag_offset];
		if ( ( rc = usb_get_string_descriptor ( usb, index, 0, data,
							len ) ) < 0 ) {
			return rc;
		}
		if ( ! setting->type )
			setting->type = &setting_type_string;
		return rc;
	}

	/* Limit length */
	if ( tag_offset > sizeof ( usb->device ) ) {
		tag_len = 0;
	} else if ( ( tag_offset + tag_len ) > sizeof ( usb->device ) ) {
		tag_len = ( sizeof ( usb->device ) - tag_offset );
	}

	/* Copy data, reversing endianness if applicable */
	dst = data;
	src = ( desc + tag_offset );
	if ( tag_direction < 0 )
		src += ( tag_len - 1 );
	if ( len > tag_len )
		len = tag_len;
	for ( ; len-- ; src += tag_direction, dst++ )
		*dst = *src;

	/* Set type to ":hexraw" if not already specified */
	if ( ! setting->type )
		setting->type = &setting_type_hexraw;

	return tag_len;
}

/** USB device settings operations */
static struct settings_operations usb_settings_operations = {
	.applies = usb_settings_applies,
	.fetch = usb_settings_fetch,
};

/** USB device settings */
static struct settings usb_settings = {
	.refcnt = NULL,
	.siblings = LIST_HEAD_INIT ( usb_settings.siblings ),
	.children = LIST_HEAD_INIT ( usb_settings.children ),
	.op = &usb_settings_operations,
	.default_scope = &usb_settings_scope,
};

/** Initialise USB device settings */
static void usb_settings_init ( void ) {
	int rc;

	if ( ( rc = register_settings ( &usb_settings, NULL, "usb" ) ) != 0 ) {
		DBG ( "USB could not register settings: %s\n",
		      strerror ( rc ) );
		return;
	}
}

/** USB device settings initialiser */
struct init_fn usb_settings_init_fn __init_fn ( INIT_NORMAL ) = {
	.initialise = usb_settings_init,
};
