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
#include <ipxe/efi/IndustryStandard/PeImage.h>

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
 * Name protocol open attributes
 *
 * @v attributes	Protocol open attributes
 * @ret name		Protocol open attributes name
 *
 * Returns a (static) string with characters for each set bit
 * corresponding to BY_(H)ANDLE_PROTOCOL, (G)ET_PROTOCOL,
 * (T)EST_PROTOCOL, BY_(C)HILD_CONTROLLER, BY_(D)RIVER, and
 * E(X)CLUSIVE.
 */
static const char * efi_open_attributes_name ( unsigned int attributes ) {
	static char attribute_chars[] = "HGTCDX";
	static char name[ sizeof ( attribute_chars ) ];
	char *tmp = name;
	unsigned int i;

	for ( i = 0 ; i < ( sizeof ( attribute_chars ) - 1 ) ; i++ ) {
		if ( attributes & ( 1 << i ) )
			*(tmp++) = attribute_chars[i];
	}
	*tmp = '\0';

	return name;
}

/**
 * Print list of openers of a given protocol on a given handle
 *
 * @v handle		EFI handle
 * @v protocol		Protocol GUID
 */
void dbg_efi_openers ( EFI_HANDLE handle, EFI_GUID *protocol ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *openers;
	EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *opener;
	UINTN count;
	unsigned int i;
	EFI_STATUS efirc;
	int rc;

	/* Retrieve list of openers */
	if ( ( efirc = bs->OpenProtocolInformation ( handle, protocol, &openers,
						     &count ) ) != 0 ) {
		rc = -EEFI ( efirc );
		printf ( "EFI could not retrieve openers for %s on %p: %s\n",
			 efi_guid_ntoa ( protocol ), handle, strerror ( rc ) );
		return;
	}

	/* Dump list of openers */
	for ( i = 0 ; i < count ; i++ ) {
		opener = &openers[i];
		printf ( "HANDLE %p %s %s opened %dx (%s)",
			 handle, efi_handle_name ( handle ),
			 efi_guid_ntoa ( protocol ), opener->OpenCount,
			 efi_open_attributes_name ( opener->Attributes ) );
		printf ( " by %p %s", opener->AgentHandle,
			 efi_handle_name ( opener->AgentHandle ) );
		if ( opener->ControllerHandle == handle ) {
			printf ( "\n" );
		} else {
			printf ( " for %p %s\n", opener->ControllerHandle,
				 efi_handle_name ( opener->ControllerHandle ) );
		}
	}

	/* Free list */
	bs->FreePool ( openers );
}

/**
 * Print list of protocol handlers attached to a handle
 *
 * @v handle		EFI handle
 */
void dbg_efi_protocols ( EFI_HANDLE handle ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_GUID **protocols;
	EFI_GUID *protocol;
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
	for ( i = 0 ; i < count ; i++ ) {
		protocol = protocols[i];
		printf ( "HANDLE %p %s %s supported\n",
			 handle, efi_handle_name ( handle ),
			 efi_guid_ntoa ( protocol ) );
		dbg_efi_openers ( handle, protocol );
	}

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
 * Get driver name
 *
 * @v wtf		Component name protocol
 * @ret name		Driver name, or NULL
 */
static const char * efi_driver_name ( EFI_COMPONENT_NAME2_PROTOCOL *wtf ) {
	static char name[64];
	CHAR16 *driver_name;
	EFI_STATUS efirc;

	/* Try "en" first; if that fails then try the first language */
	if ( ( ( efirc = wtf->GetDriverName ( wtf, "en",
					      &driver_name ) ) != 0 ) &&
	     ( ( efirc = wtf->GetDriverName ( wtf, wtf->SupportedLanguages,
					      &driver_name ) ) != 0 ) ) {
		return NULL;
	}

	/* Convert name from CHAR16 to char */
	snprintf ( name, sizeof ( name ), "%ls", driver_name );
	return name;
}

/**
 * Get PE/COFF debug filename
 *
 * @v loaded		Loaded image
 * @ret name		PE/COFF debug filename, or NULL
 */
static const char *
efi_pecoff_debug_name ( EFI_LOADED_IMAGE_PROTOCOL *loaded ) {
	static char buf[32];
	EFI_IMAGE_DOS_HEADER *dos = loaded->ImageBase;
	EFI_IMAGE_OPTIONAL_HEADER_UNION *pe;
	EFI_IMAGE_OPTIONAL_HEADER32 *opt32;
	EFI_IMAGE_OPTIONAL_HEADER64 *opt64;
	EFI_IMAGE_DATA_DIRECTORY *datadir;
	EFI_IMAGE_DEBUG_DIRECTORY_ENTRY *debug;
	EFI_IMAGE_DEBUG_CODEVIEW_NB10_ENTRY *codeview;
	size_t max_len;
	char *name;
	char *tmp;

	/* Parse DOS header */
	if ( ! dos ) {
		DBGC ( loaded, "Missing DOS header\n" );
		return NULL;
	}
	if ( dos->e_magic != EFI_IMAGE_DOS_SIGNATURE ) {
		DBGC ( loaded, "Bad DOS signature\n" );
		return NULL;
	}
	pe = ( loaded->ImageBase + dos->e_lfanew );

	/* Parse PE header */
	if ( pe->Pe32.Signature != EFI_IMAGE_NT_SIGNATURE ) {
		DBGC ( loaded, "Bad PE signature\n" );
		return NULL;
	}
	opt32 = &pe->Pe32.OptionalHeader;
	opt64 = &pe->Pe32Plus.OptionalHeader;
	if ( opt32->Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC ) {
		datadir = opt32->DataDirectory;
	} else if ( opt64->Magic == EFI_IMAGE_NT_OPTIONAL_HDR64_MAGIC ) {
		datadir = opt64->DataDirectory;
	} else {
		DBGC ( loaded, "Bad optional header signature\n" );
		return NULL;
	}

	/* Parse data directory entry */
	if ( ! datadir[EFI_IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress ) {
		DBGC ( loaded, "Empty debug directory entry\n" );
		return NULL;
	}
	debug = ( loaded->ImageBase +
		  datadir[EFI_IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress );

	/* Parse debug directory entry */
	if ( debug->Type != EFI_IMAGE_DEBUG_TYPE_CODEVIEW ) {
		DBGC ( loaded, "Not a CodeView debug directory entry\n" );
		return NULL;
	}
	codeview = ( loaded->ImageBase + debug->RVA );

	/* Parse CodeView entry */
	if ( codeview->Signature != CODEVIEW_SIGNATURE_NB10 ) {
		DBGC ( loaded, "Bad CodeView signature\n" );
		return NULL;
	}
	name = ( ( ( void * ) codeview ) + sizeof ( *codeview ) );

	/* Sanity check - avoid scanning endlessly through memory */
	max_len = EFI_PAGE_SIZE; /* Reasonably sane */
	if ( strnlen ( name, max_len ) == max_len ) {
		DBGC ( loaded, "Excessively long or invalid CodeView name\n" );
		return NULL;
	}

	/* Skip any directory components.  We cannot modify this data
	 * or create a temporary buffer, so do not use basename().
	 */
	while ( ( ( tmp = strchr ( name, '/' ) ) != NULL ) ||
		( ( tmp = strchr ( name, '\\' ) ) != NULL ) ) {
		name = ( tmp + 1 );
	}

	/* Copy base name to buffer */
	snprintf ( buf, sizeof ( buf ), "%s", name );

	/* Strip file suffix, if present */
	if ( ( tmp = strrchr ( name, '.' ) ) != NULL )
		*tmp = '\0';

	return name;
}

/**
 * Get initial loaded image name
 *
 * @v loaded		Loaded image
 * @ret name		Initial loaded image name, or NULL
 */
static const char *
efi_first_loaded_image_name ( EFI_LOADED_IMAGE_PROTOCOL *loaded ) {

	return ( ( loaded->ParentHandle == NULL ) ? "DxeCore(?)" : NULL );
}

/**
 * Get loaded image name from file path
 *
 * @v loaded		Loaded image
 * @ret name		Loaded image name, or NULL
 */
static const char *
efi_loaded_image_filepath_name ( EFI_LOADED_IMAGE_PROTOCOL *loaded ) {

	return efi_devpath_text ( loaded->FilePath );
}

/** An EFI handle name type */
struct efi_handle_name_type {
	/** Protocol */
	EFI_GUID *protocol;
	/**
	 * Get name
	 *
	 * @v interface		Protocol interface
	 * @ret name		Name of handle, or NULL on failure
	 */
	const char * ( * name ) ( void *interface );
};

/**
 * Define an EFI handle name type
 *
 * @v protocol		Protocol interface
 * @v name		Method to get name
 * @ret type		EFI handle name type
 */
#define EFI_HANDLE_NAME_TYPE( protocol, name ) {	\
	(protocol),					\
	( const char * ( * ) ( void * ) ) (name),	\
	}

/** EFI handle name types */
static struct efi_handle_name_type efi_handle_name_types[] = {
	/* Device path */
	EFI_HANDLE_NAME_TYPE ( &efi_device_path_protocol_guid,
			       efi_devpath_text ),
	/* Driver name (for driver image handles) */
	EFI_HANDLE_NAME_TYPE ( &efi_component_name2_protocol_guid,
			       efi_driver_name ),
	/* PE/COFF debug filename (for image handles) */
	EFI_HANDLE_NAME_TYPE ( &efi_loaded_image_protocol_guid,
			       efi_pecoff_debug_name ),
	/* Loaded image device path (for image handles) */
	EFI_HANDLE_NAME_TYPE ( &efi_loaded_image_device_path_protocol_guid,
			       efi_devpath_text ),
	/* First loaded image name (for the DxeCore image) */
	EFI_HANDLE_NAME_TYPE ( &efi_loaded_image_protocol_guid,
			       efi_first_loaded_image_name ),
	/* Handle's loaded image file path (for image handles) */
	EFI_HANDLE_NAME_TYPE ( &efi_loaded_image_protocol_guid,
			       efi_loaded_image_filepath_name ),
};

/**
 * Get name of an EFI handle
 *
 * @v handle		EFI handle
 * @ret text		Name of handle, or NULL
 */
const char * efi_handle_name ( EFI_HANDLE handle ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct efi_handle_name_type *type;
	unsigned int i;
	void *interface;
	const char *name;
	EFI_STATUS efirc;

	/* Fail immediately for NULL handles */
	if ( ! handle )
		return NULL;

	/* Try each name type in turn */
	for ( i = 0 ; i < ( sizeof ( efi_handle_name_types ) /
			    sizeof ( efi_handle_name_types[0] ) ) ; i++ ) {
		type = &efi_handle_name_types[i];

		/* Try to open the applicable protocol */
		efirc = bs->OpenProtocol ( handle, type->protocol, &interface,
					   efi_image_handle, handle,
					   EFI_OPEN_PROTOCOL_GET_PROTOCOL );
		if ( efirc != 0 )
			continue;

		/* Try to get name from this protocol */
		name = type->name ( interface );

		/* Close protocol */
		bs->CloseProtocol ( handle, type->protocol,
				    efi_image_handle, handle );

		/* Use this name, if possible */
		if ( name && name[0] )
			return name;
	}

	return "UNKNOWN";
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
