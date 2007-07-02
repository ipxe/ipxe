/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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

/**
 * @file
 *
 * PXE image format
 *
 */

#include <pxe.h>
#include <pxe_call.h>
#include <gpxe/uaccess.h>
#include <gpxe/image.h>
#include <gpxe/segment.h>
#include <gpxe/netdevice.h>

struct image_type pxe_image_type __image_type ( PROBE_PXE );

/**
 * Execute PXE image
 *
 * @v image		PXE image
 * @ret rc		Return status code
 */
static int pxe_exec ( struct image *image __unused ) {
	struct net_device *netdev;
	int rc;

	/* Ensure that PXE stack is ready to use */
	pxe_init_structures();
	pxe_hook_int1a();

	/* Arbitrarily pick the first open network device to use for PXE */
	for_each_netdev ( netdev ) {
		pxe_set_netdev ( netdev );
		break;
	}

	/* Start PXE NBP */
	rc = pxe_start_nbp();

	/* Deactivate PXE */
	pxe_set_netdev ( NULL );
	pxe_unhook_int1a();

	return rc;
}

/**
 * Load PXE image into memory
 *
 * @v image		PXE file
 * @ret rc		Return status code
 */
int pxe_load ( struct image *image ) {
	userptr_t buffer = real_to_user ( 0, 0x7c00 );
	size_t filesz = image->len;
	size_t memsz = image->len;
	int rc;

	/* There are no signature checks for PXE; we will accept anything */
	if ( ! image->type )
		image->type = &pxe_image_type;

	/* Verify and prepare segment */
	if ( ( rc = prep_segment ( buffer, filesz, memsz ) ) != 0 ) {
		DBG ( "PXE image could not prepare segment: %s\n",
		      strerror ( rc ) );
		return rc;
	}

	/* Copy image to segment */
	memcpy_user ( buffer, 0, image->data, 0, filesz );

	return 0;
}

/** PXE image type */
struct image_type pxe_image_type __image_type ( PROBE_PXE ) = {
	.name = "PXE",
	.load = pxe_load,
	.exec = pxe_exec,
};
