/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <realmode.h>
#include <sdi.h>
#include <ipxe/image.h>
#include <ipxe/features.h>

/** @file
 *
 * System Deployment Image (SDI)
 *
 * Based on the MSDN article "RAM boot using SDI in Windows XP
 * Embedded with Service Pack 1", available at the time of writing
 * from:
 *
 *   http://msdn.microsoft.com/en-us/library/ms838543.aspx
 */

FEATURE ( FEATURE_IMAGE, "SDI", DHCP_EB_FEATURE_SDI, 1 );

/**
 * Execute SDI image
 *
 * @v image		SDI file
 * @ret rc		Return status code
 */
static int sdi_exec ( struct image *image ) {
	const struct sdi_header *sdi;
	uint32_t sdiptr;

	/* Sanity check */
	assert ( image->len >= sizeof ( *sdi ) );
	sdi = image->data;

	/* Check that image is bootable */
	if ( sdi->boot_size == 0 ) {
		DBGC ( image, "SDI %s is not bootable\n", image->name );
		return -ENOTTY;
	}
	DBGC ( image, "SDI %s image at %08lx+%08zx\n",
	       image->name, virt_to_phys ( image->data ), image->len );
	DBGC ( image, "SDI %s boot code at %08llx+%llx\n", image->name,
	       ( virt_to_phys ( image->data ) + sdi->boot_offset ),
	       sdi->boot_size );

	/* Copy boot code */
	memcpy ( real_to_virt ( SDI_BOOT_SEG, SDI_BOOT_OFF ),
		 ( image->data + sdi->boot_offset ), sdi->boot_size );

	/* Jump to boot code */
	sdiptr = ( virt_to_phys ( image->data ) | SDI_WTF );
	__asm__ __volatile__ ( REAL_CODE ( "ljmp %0, %1\n\t" )
			       : : "i" ( SDI_BOOT_SEG ),
				   "i" ( SDI_BOOT_OFF ),
				   "d" ( sdiptr ) );

	/* There is no way for the image to return, since we provide
	 * no return address.
	 */
	assert ( 0 );

	return -ECANCELED; /* -EIMPOSSIBLE */
}

/**
 * Probe SDI image
 *
 * @v image		SDI file
 * @ret rc		Return status code
 */
static int sdi_probe ( struct image *image ) {
	const struct sdi_header *sdi;

	/* Sanity check */
	if ( image->len < sizeof ( *sdi ) ) {
		DBGC ( image, "SDI %s too short for SDI header\n",
		       image->name );
		return -ENOEXEC;
	}
	sdi = image->data;

	/* Check signature */
	if ( sdi->magic != SDI_MAGIC ) {
		DBGC ( image, "SDI %s is not an SDI image\n",
		       image->name );
		return -ENOEXEC;
	}

	return 0;
}

/** SDI image type */
struct image_type sdi_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "SDI",
	.probe = sdi_probe,
	.exec = sdi_exec,
};
