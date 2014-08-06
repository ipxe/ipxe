/*
 * Copyright (C) 2011 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <string.h>
#include <errno.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_utils.h>

/** @file
 *
 * EFI utilities
 *
 */

/**
 * Find end of device path
 *
 * @v path		Path to device
 * @ret path_end	End of device path
 */
EFI_DEVICE_PATH_PROTOCOL * efi_devpath_end ( EFI_DEVICE_PATH_PROTOCOL *path ) {

	while ( path->Type != END_DEVICE_PATH_TYPE ) {
		path = ( ( ( void * ) path ) +
			 /* There's this amazing new-fangled thing known as
			  * a UINT16, but who wants to use one of those? */
			 ( ( path->Length[1] << 8 ) | path->Length[0] ) );
	}

	return path;
}

/**
 * Add EFI device as child of another EFI device
 *
 * @v parent		EFI parent device handle
 * @v child		EFI child device handle
 * @ret rc		Return status code
 */
int efi_child_add ( EFI_HANDLE parent, EFI_HANDLE child ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *devpath;
	EFI_STATUS efirc;
	int rc;

	/* Re-open the device path protocol */
	if ( ( efirc = bs->OpenProtocol ( parent,
					  &efi_device_path_protocol_guid,
					  &devpath,
					  efi_image_handle, child,
					  EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
					  ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( parent, "EFIDEV %p %s could not add child",
		       parent, efi_handle_name ( parent ) );
		DBGC ( parent, " %p %s: %s\n", child,
		       efi_handle_name ( child ), strerror ( rc ) );
		DBGC_EFI_OPENERS ( parent, parent,
				   &efi_device_path_protocol_guid );
		return rc;
	}

	DBGC2 ( parent, "EFIDEV %p %s added child",
		parent, efi_handle_name ( parent ) );
	DBGC2 ( parent, " %p %s\n", child, efi_handle_name ( child ) );
	return 0;
}

/**
 * Remove EFI device as child of another EFI device
 *
 * @v parent		EFI parent device handle
 * @v child		EFI child device handle
 */
void efi_child_del ( EFI_HANDLE parent, EFI_HANDLE child ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;

	bs->CloseProtocol ( parent, &efi_device_path_protocol_guid,
			    efi_image_handle, child );
	DBGC2 ( parent, "EFIDEV %p %s removed child",
		parent, efi_handle_name ( parent ) );
	DBGC2 ( parent, " %p %s\n",
		child, efi_handle_name ( child ) );
}
