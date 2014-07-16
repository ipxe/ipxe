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
#include <ipxe/efi/Protocol/BlockIo.h>
#include <ipxe/efi/Protocol/DiskIo.h>
#include <ipxe/efi/Protocol/SimpleFileSystem.h>
#include <ipxe/efi/Protocol/SimpleNetwork.h>

/** Device path protocol GUID */
static EFI_GUID efi_device_path_protocol_guid
	= EFI_DEVICE_PATH_PROTOCOL_GUID;

/** Device path to text protocol */
static EFI_DEVICE_PATH_TO_TEXT_PROTOCOL *efidpt;
EFI_REQUEST_PROTOCOL ( EFI_DEVICE_PATH_TO_TEXT_PROTOCOL, &efidpt );

/** A well-known GUID */
struct efi_well_known_guid {
	/** GUID */
	EFI_GUID guid;
	/** Name */
	const char *name;
};

/** Well-known GUIDs */
static struct efi_well_known_guid efi_well_known_guids[] = {
	{ EFI_BLOCK_IO_PROTOCOL_GUID,		"BlockIo" },
	{ EFI_DISK_IO_PROTOCOL_GUID,		"DiskIo" },
	{ EFI_DEVICE_PATH_PROTOCOL_GUID,	"DevicePath" },
	{ EFI_LOADED_IMAGE_PROTOCOL_GUID,	"LoadedImage" },
	{ EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID,	"SimpleFileSystem" },
	{ EFI_SIMPLE_NETWORK_PROTOCOL_GUID,	"SimpleNetwork" },
};

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
	unsigned int i;

	/* Check for a match against well-known GUIDs */
	for ( i = 0 ; i < ( sizeof ( efi_well_known_guids ) /
			    sizeof ( efi_well_known_guids[0] ) ) ; i++ ) {
		if ( memcmp ( guid, &efi_well_known_guids[i].guid,
			      sizeof ( *guid ) ) == 0 ) {
			return efi_well_known_guids[i].name;
		}
	}

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
 * Get textual representation of device path
 *
 * @v path		Device path
 * @ret text		Textual representation of device path, or NULL
 */
const char * efi_devpath_text ( EFI_DEVICE_PATH_PROTOCOL *path ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	static char text[256];
	CHAR16 *wtext;

	/* Convert path to a textual representation */
	if ( ! efidpt )
		return NULL;
	wtext = efidpt->ConvertDevicePathToText ( path, TRUE, FALSE );
	if ( ! wtext )
		return NULL;

	/* Store path in buffer */
	snprintf ( text, sizeof ( text ), "%ls", wtext );

	/* Free path */
	bs->FreePool ( wtext );

	return text;
}

/**
 * Get textual representation of device path for a handle
 *
 * @v handle		EFI handle
 * @ret text		Textual representation of device path, or NULL
 */
const char * efi_handle_devpath_text ( EFI_HANDLE handle ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	union {
		EFI_DEVICE_PATH_PROTOCOL *path;
		void *interface;
	} path;
	const char *text;
	EFI_STATUS efirc;

	/* Obtain device path, if any */
	if ( ( efirc = bs->OpenProtocol ( handle,
					  &efi_device_path_protocol_guid,
					  &path.interface, efi_image_handle,
					  handle,
					  EFI_OPEN_PROTOCOL_GET_PROTOCOL ))!=0){
		return NULL;
	}

	/* Format device path */
	text = efi_devpath_text ( path.path );

	/* Close device path */
	bs->CloseProtocol ( handle, &efi_device_path_protocol_guid,
			    efi_image_handle, handle );

	return text;
}
