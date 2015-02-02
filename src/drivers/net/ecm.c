/*
 * Copyright (C) 2014 Michael Brown <mbrown@fensystems.co.uk>.
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
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <errno.h>
#include <ipxe/if_ether.h>
#include <ipxe/base16.h>
#include <ipxe/usb.h>
#include "ecm.h"

/** @file
 *
 * CDC-ECM USB Ethernet driver
 *
 */

/**
 * Locate Ethernet functional descriptor
 *
 * @v config		Configuration descriptor
 * @v interface		Interface descriptor
 * @ret desc		Descriptor, or NULL if not found
 */
struct ecm_ethernet_descriptor *
ecm_ethernet_descriptor ( struct usb_configuration_descriptor *config,
			  struct usb_interface_descriptor *interface ) {
	struct ecm_ethernet_descriptor *desc;

	for_each_interface_descriptor ( desc, config, interface ) {
		if ( ( desc->header.type == USB_CS_INTERFACE_DESCRIPTOR ) &&
		     ( desc->subtype == CDC_SUBTYPE_ETHERNET ) )
			return desc;
	}
	return NULL;
}

/**
 * Get hardware MAC address
 *
 * @v usb		USB device
 * @v desc		Ethernet functional descriptor
 * @v hw_addr		Hardware address to fill in
 * @ret rc		Return status code
 */
int ecm_fetch_mac ( struct usb_device *usb,
		    struct ecm_ethernet_descriptor *desc, uint8_t *hw_addr ) {
	char buf[ base16_encoded_len ( ETH_ALEN ) + 1 /* NUL */ ];
	int len;
	int rc;

	/* Fetch MAC address string */
	len = usb_get_string_descriptor ( usb, desc->mac, 0, buf,
					  sizeof ( buf ) );
	if ( len < 0 ) {
		rc = len;
		return rc;
	}

	/* Sanity check */
	if ( len != ( ( int ) ( sizeof ( buf ) - 1 /* NUL */ ) ) )
		return -EINVAL;

	/* Decode MAC address */
	len = base16_decode ( buf, hw_addr );
	if ( len < 0 ) {
		rc = len;
		return rc;
	}

	return 0;
}
