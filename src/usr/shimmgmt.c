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

#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_shim.h>
#include <usr/shimmgmt.h>

/** @file
 *
 * EFI shim management
 *
 */

/**
 * Set shim image
 *
 * @v image		Shim image, or NULL to clear shim
 * @v require_loader	Require use of a third party loader
 * @v allow_pxe		Allow use of PXE base code
 * @v allow_sbat	Allow SBAT variable access
 * @ret rc		Return status code
 */
int shim ( struct image *image, int require_loader, int allow_pxe,
	   int allow_sbat ) {

	/* Record (or clear) shim image */
	image_tag ( image, &efi_shim );

	/* Avoid including image in constructed initrd */
	if ( image )
		image_hide ( image );

	/* Record configuration */
	efi_shim_require_loader = require_loader;
	efi_shim_allow_pxe = allow_pxe;
	efi_shim_allow_sbat = allow_sbat;

	return 0;
}
