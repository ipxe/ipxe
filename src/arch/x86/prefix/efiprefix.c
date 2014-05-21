/*
 * Copyright (C) 2009 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <errno.h>
#include <ipxe/efi/efi.h>

/**
 * EFI entry point
 *
 * @v image_handle	Image handle
 * @v systab		System table
 * @ret efirc		EFI return status code
 */
EFI_STATUS EFIAPI _efi_start ( EFI_HANDLE image_handle,
			       EFI_SYSTEM_TABLE *systab ) {
	EFI_STATUS efirc;
	int rc;

	/* Initialise EFI environment */
	if ( ( efirc = efi_init ( image_handle, systab ) ) != 0 )
		goto err_init;

	/* Call to main() */
	if ( ( rc = main() ) != 0 ) {
		efirc = EFIRC ( rc );
		goto err_main;
	}

 err_main:
	efi_loaded_image->Unload ( image_handle );
 err_init:
	return efirc;
}
