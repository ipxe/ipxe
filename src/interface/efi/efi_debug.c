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
#include <ipxe/efi/Protocol/BlockIo.h>
#include <ipxe/efi/Protocol/BusSpecificDriverOverride.h>
#include <ipxe/efi/Protocol/ComponentName.h>
#include <ipxe/efi/Protocol/ComponentName2.h>
#include <ipxe/efi/Protocol/DevicePath.h>
#include <ipxe/efi/Protocol/DevicePathToText.h>
#include <ipxe/efi/Protocol/DiskIo.h>
#include <ipxe/efi/Protocol/DriverBinding.h>
#include <ipxe/efi/Protocol/LoadFile.h>
#include <ipxe/efi/Protocol/LoadFile2.h>
#include <ipxe/efi/Protocol/LoadedImage.h>
#include <ipxe/efi/Protocol/PciIo.h>
#include <ipxe/efi/Protocol/PciRootBridgeIo.h>
#include <ipxe/efi/Protocol/SimpleFileSystem.h>
#include <ipxe/efi/Protocol/SimpleNetwork.h>

/** Block I/O protocol GUID */
static EFI_GUID efi_block_io_protocol_guid
	= EFI_BLOCK_IO_PROTOCOL_GUID;

/** Bus specific driver override protocol GUID */
static EFI_GUID efi_bus_specific_driver_override_protocol_guid
	= EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_PROTOCOL_GUID;

/** Component name protocol GUID */
static EFI_GUID efi_component_name_protocol_guid
	= EFI_COMPONENT_NAME_PROTOCOL_GUID;

/** Component name 2 protocol GUID */
static EFI_GUID efi_component_name2_protocol_guid
	= EFI_COMPONENT_NAME2_PROTOCOL_GUID;

/** Device path protocol GUID */
static EFI_GUID efi_device_path_protocol_guid
	= EFI_DEVICE_PATH_PROTOCOL_GUID;

/** Disk I/O protocol GUID */
static EFI_GUID efi_disk_io_protocol_guid
	= EFI_DISK_IO_PROTOCOL_GUID;

/** Driver binding protocol GUID */
static EFI_GUID efi_driver_binding_protocol_guid
	= EFI_DRIVER_BINDING_PROTOCOL_GUID;

/** Load file protocol GUID */
static EFI_GUID efi_load_file_protocol_guid
	= EFI_LOAD_FILE_PROTOCOL_GUID;

/** Load file 2 protocol GUID */
static EFI_GUID efi_load_file2_protocol_guid
	= EFI_LOAD_FILE2_PROTOCOL_GUID;

/** Loaded image protocol GUID */
static EFI_GUID efi_loaded_image_protocol_guid
	= EFI_LOADED_IMAGE_PROTOCOL_GUID;

/** Loaded image device path protocol GUID */
static EFI_GUID efi_loaded_image_device_path_protocol_guid
	= EFI_LOADED_IMAGE_DEVICE_PATH_PROTOCOL_GUID;

/** PCI I/O protocol GUID */
static EFI_GUID efi_pci_io_protocol_guid
	= EFI_PCI_IO_PROTOCOL_GUID;

/** PCI root bridge I/O protocol GUID */
static EFI_GUID efi_pci_root_bridge_io_protocol_guid
	= EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_GUID;

/** Simple file system protocol GUID */
static EFI_GUID efi_simple_file_system_protocol_guid
	= EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

/** Simple network protocol guid */
static EFI_GUID efi_simple_network_protocol_guid
	= EFI_SIMPLE_NETWORK_PROTOCOL_GUID;

/** Device path to text protocol */
static EFI_DEVICE_PATH_TO_TEXT_PROTOCOL *efidpt;
EFI_REQUEST_PROTOCOL ( EFI_DEVICE_PATH_TO_TEXT_PROTOCOL, &efidpt );

/** A well-known GUID */
struct efi_well_known_guid {
	/** GUID */
	EFI_GUID *guid;
	/** Name */
	const char *name;
};

/** Well-known GUIDs */
static struct efi_well_known_guid efi_well_known_guids[] = {
	{ &efi_block_io_protocol_guid,
	  "BlockIo" },
	{ &efi_bus_specific_driver_override_protocol_guid,
	  "BusSpecificDriverOverride" },
	{ &efi_component_name2_protocol_guid,
	  "ComponentName2" },
	{ &efi_component_name_protocol_guid,
	  "ComponentName" },
	{ &efi_device_path_protocol_guid,
	  "DevicePath" },
	{ &efi_driver_binding_protocol_guid,
	  "DriverBinding" },
	{ &efi_disk_io_protocol_guid,
	  "DiskIo" },
	{ &efi_load_file_protocol_guid,
	  "LoadFile" },
	{ &efi_load_file2_protocol_guid,
	  "LoadFile2" },
	{ &efi_loaded_image_protocol_guid,
	  "LoadedImage" },
	{ &efi_loaded_image_device_path_protocol_guid,
	  "LoadedImageDevicePath"},
	{ &efi_pci_io_protocol_guid,
	  "PciIo" },
	{ &efi_pci_root_bridge_io_protocol_guid,
	  "PciRootBridgeIo" },
	{ &efi_simple_file_system_protocol_guid,
	  "SimpleFileSystem" },
	{ &efi_simple_network_protocol_guid,
	  "SimpleNetwork" },
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
		if ( memcmp ( guid, efi_well_known_guids[i].guid,
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
