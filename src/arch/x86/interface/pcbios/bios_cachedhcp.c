/*
 * Copyright (C) 2013 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdint.h>
#include <string.h>
#include <ipxe/init.h>
#include <ipxe/cachedhcp.h>
#include <realmode.h>
#include <pxe_api.h>

/** @file
 *
 * Cached DHCP packet
 *
 */

/** Cached DHCPACK physical address
 *
 * This can be set by the prefix.
 */
uint32_t __bss16 ( cached_dhcpack_phys );
#define cached_dhcpack_phys __use_data16 ( cached_dhcpack_phys )

/** Colour for debug messages */
#define colour &cached_dhcpack_phys

/**
 * Cached DHCPACK initialisation function
 *
 */
static void cachedhcp_init ( void ) {
	int rc;

	/* Do nothing if no cached DHCPACK is present */
	if ( ! cached_dhcpack_phys ) {
		DBGC ( colour, "CACHEDHCP found no cached DHCPACK\n" );
		return;
	}

	/* Record cached DHCPACK */
	if ( ( rc = cachedhcp_record ( &cached_dhcpack, 0,
				       phys_to_virt ( cached_dhcpack_phys ),
				       sizeof ( BOOTPLAYER_t ) ) ) != 0 ) {
		DBGC ( colour, "CACHEDHCP could not record DHCPACK: %s\n",
		       strerror ( rc ) );
		return;
	}

	/* Mark as consumed */
	cached_dhcpack_phys = 0;
}

/** Cached DHCPACK initialisation function */
struct init_fn cachedhcp_init_fn __init_fn ( INIT_NORMAL ) = {
	.initialise = cachedhcp_init,
};
