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
 */

FILE_LICENCE ( GPL2_OR_LATER );

/**
 * @file
 *
 * EFI debugging utilities
 *
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ipxe/uuid.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/Protocol/DevicePath.h>
#include <ipxe/efi/Protocol/DevicePathToText.h>

/** Device path to text protocol */
static EFI_DEVICE_PATH_TO_TEXT_PROTOCOL *efidpt;
EFI_REQUIRE_PROTOCOL ( EFI_DEVICE_PATH_TO_TEXT_PROTOCOL, &efidpt );

/**
 * Convert GUID to a printable string
 *
 * @v guid		GUID
 * @ret string		Printable string
 */
const char * efi_guid_ntoa ( EFI_GUID *guid ) {
	union {
		union uuid uuid;
		EFI_GUID guid;
	} u;

	/* Convert GUID to standard endianness */
	memcpy ( &u.guid, guid, sizeof ( u.guid ) );
	uuid_mangle ( &u.uuid );
	return uuid_ntoa ( &u.uuid );
}

/**
 * Print list of protocol handlers attached to a handle
 *
 * @v handle		EFI handle
 */
void dbg_efi_protocols ( EFI_HANDLE handle ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
        EFI_GUID **protocols;
	UINTN count;
	unsigned int i;
	EFI_STATUS efirc;
	int rc;

	/* Retrieve list of protocols */
	if ( ( efirc = bs->ProtocolsPerHandle ( handle, &protocols,
						&count ) ) != 0 ) {
		rc = -EEFI ( efirc );
		printf ( "EFI could not retrieve protocols for %p: %s\n",
			 handle, strerror ( rc ) );
		return;
	}

	/* Dump list of protocols */
	for ( i = 0 ; i < count ; i++ )
		printf ( "%s\n", efi_guid_ntoa ( protocols[i] ) );

	/* Free list */
	bs->FreePool ( protocols );
}

/**
 * Print device path
 *
 * @v path		Device path
 */
void dbg_efi_devpath ( EFI_DEVICE_PATH_PROTOCOL *path ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_DEVICE_PATH_PROTOCOL *end;
	CHAR16 *text;
	size_t len;

	/* Convert path to a textual representation */
	text = efidpt->ConvertDevicePathToText ( path, TRUE, FALSE );
	if ( ! text ) {
		printf ( "<cannot convert>:\n" );
		end = efi_devpath_end ( path );
		len = ( ( ( void * ) end ) - ( ( void * ) path ) +
			sizeof ( *end ) );
		dbg_hex_dump_da ( 0, path, len );
		return;
	}

	/* Print path */
	printf ( "%ls", text );

	/* Free path */
	bs->FreePool ( text );
}
