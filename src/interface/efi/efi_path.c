/*
 * Copyright (C) 2020 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_path.h>

/** @file
 *
 * EFI device paths
 *
 */

/**
 * Find end of device path
 *
 * @v path		Path to device
 * @ret path_end	End of device path
 */
EFI_DEVICE_PATH_PROTOCOL * efi_path_end ( EFI_DEVICE_PATH_PROTOCOL *path ) {

	while ( path->Type != END_DEVICE_PATH_TYPE ) {
		path = ( ( ( void * ) path ) +
			 /* There's this amazing new-fangled thing known as
			  * a UINT16, but who wants to use one of those? */
			 ( ( path->Length[1] << 8 ) | path->Length[0] ) );
	}

	return path;
}

/**
 * Find length of device path (excluding terminator)
 *
 * @v path		Path to device
 * @ret path_len	Length of device path
 */
size_t efi_path_len ( EFI_DEVICE_PATH_PROTOCOL *path ) {
	EFI_DEVICE_PATH_PROTOCOL *end = efi_path_end ( path );

	return ( ( ( void * ) end ) - ( ( void * ) path ) );
}

/**
 * Describe object as an EFI device path
 *
 * @v intf		Interface
 * @ret path		EFI device path, or NULL
 *
 * The caller is responsible for eventually calling free() on the
 * allocated device path.
 */
EFI_DEVICE_PATH_PROTOCOL * efi_describe ( struct interface *intf ) {
	struct interface *dest;
	efi_describe_TYPE ( void * ) *op =
		intf_get_dest_op ( intf, efi_describe, &dest );
	void *object = intf_object ( dest );
	EFI_DEVICE_PATH_PROTOCOL *path;

	if ( op ) {
		path = op ( object );
	} else {
		path = NULL;
	}

	intf_put ( dest );
	return path;
}
