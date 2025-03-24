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
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/**
 * @file
 *
 * EFI debugging utilities
 *
 */

#include <stdio.h>
#include <errno.h>
#include <ipxe/base16.h>
#include <ipxe/vsprintf.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_path.h>
#include <ipxe/efi/Protocol/ComponentName.h>
#include <ipxe/efi/Protocol/ComponentName2.h>
#include <ipxe/efi/Protocol/DriverBinding.h>
#include <ipxe/efi/Protocol/DevicePathToText.h>
#include <ipxe/efi/IndustryStandard/PeImage.h>

/** Device path to text protocol */
static EFI_DEVICE_PATH_TO_TEXT_PROTOCOL *efidpt;
EFI_REQUEST_PROTOCOL ( EFI_DEVICE_PATH_TO_TEXT_PROTOCOL, &efidpt );

/**
 * Name locate search type
 *
 * @v search_type	Locate search type
 * @ret name		Locate search type name
 */
const __attribute__ (( pure )) char *
efi_locate_search_type_name ( EFI_LOCATE_SEARCH_TYPE search_type ) {
	static char buf[16];

	switch ( search_type ) {
	case AllHandles :	return "AllHandles";
	case ByRegisterNotify:	return "ByRegisterNotify";
	case ByProtocol:	return "ByProtocol";
	default:
		snprintf ( buf, sizeof ( buf ), "UNKNOWN<%d>", search_type );
		return buf;
	}
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
const __attribute__ (( pure )) char *
efi_open_attributes_name ( unsigned int attributes ) {
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
 * Print opened protocol information
 *
 * @v handle		EFI handle
 * @V protocol		Protocol GUID
 * @v opener		Opened protocol information
 */
void dbg_efi_opener ( EFI_HANDLE handle, EFI_GUID *protocol,
		      EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *opener ) {

	printf ( "HANDLE %s %s opened %dx (%s)", efi_handle_name ( handle ),
		 efi_guid_ntoa ( protocol ), opener->OpenCount,
		 efi_open_attributes_name ( opener->Attributes ) );
	printf ( " by %s", efi_handle_name ( opener->AgentHandle ) );
	if ( opener->ControllerHandle == handle ) {
		printf ( "\n" );
	} else {
		printf ( " for %s\n",
			 efi_handle_name ( opener->ControllerHandle ) );
	}
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
	UINTN count;
	unsigned int i;
	EFI_STATUS efirc;
	int rc;

	/* Sanity check */
	if ( ( ! handle ) || ( ! protocol ) ) {
		printf ( "HANDLE %s could not retrieve openers for %s\n",
			 efi_handle_name ( handle ),
			 efi_guid_ntoa ( protocol ) );
		return;
	}

	/* Retrieve list of openers */
	if ( ( efirc = bs->OpenProtocolInformation ( handle, protocol, &openers,
						     &count ) ) != 0 ) {
		rc = -EEFI ( efirc );
		printf ( "HANDLE %s could not retrieve openers for %s: %s\n",
			 efi_handle_name ( handle ),
			 efi_guid_ntoa ( protocol ), strerror ( rc ) );
		return;
	}

	/* Dump list of openers */
	for ( i = 0 ; i < count ; i++ )
		dbg_efi_opener ( handle, protocol, &openers[i] );

	/* Free list */
	bs->FreePool ( openers );
}

/**
 * Print protocol information on a given handle
 *
 * @v handle		EFI handle
 * @v protocol		Protocol GUID
 */
void dbg_efi_protocol ( EFI_HANDLE handle, EFI_GUID *protocol ) {
	VOID *interface;
	int rc;

	/* Get protocol instance */
	if ( ( rc = efi_open ( handle, protocol, &interface ) ) != 0 ) {
		printf ( "HANDLE %s could not identify %s: %s\n",
			 efi_handle_name ( handle ),
			 efi_guid_ntoa ( protocol ), strerror ( rc ) );
		return;
	}
	printf ( "HANDLE %s %s at %p\n", efi_handle_name ( handle ),
		 efi_guid_ntoa ( protocol ), interface );

	/* Dump list of openers */
	dbg_efi_openers ( handle, protocol );
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

	/* Sanity check */
	if ( ! handle ) {
		printf ( "HANDLE %p could not retrieve protocols\n", handle );
		return;
	}

	/* Retrieve list of protocols */
	if ( ( efirc = bs->ProtocolsPerHandle ( handle, &protocols,
						&count ) ) != 0 ) {
		rc = -EEFI ( efirc );
		printf ( "HANDLE %s could not retrieve protocols: %s\n",
			 efi_handle_name ( handle ), strerror ( rc ) );
		return;
	}

	/* Dump list of protocols */
	for ( i = 0 ; i < count ; i++ ) {
		dbg_efi_protocol ( handle, protocols[i] );
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
const __attribute__ (( pure )) char *
efi_devpath_text ( EFI_DEVICE_PATH_PROTOCOL *path ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	static char text[512];
	size_t len;
	CHAR16 *wtext;

	/* Sanity checks */
	if ( ! path ) {
		DBG ( "[NULL DevicePath]" );
		return NULL;
	}

	/* If we have no DevicePathToText protocol then use a raw hex string */
	if ( ! efidpt ) {
		DBG ( "[No DevicePathToText]" );
		len = efi_path_len ( path );
		base16_encode ( path, len, text, sizeof ( text ) );
		return text;
	}

	/* Convert path to a textual representation */
	wtext = efidpt->ConvertDevicePathToText ( path, FALSE, FALSE );
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
static const char * efi_driver_name ( EFI_COMPONENT_NAME_PROTOCOL *wtf ) {
	static char name[64];
	CHAR16 *driver_name;
	EFI_STATUS efirc;

	/* Sanity check */
	if ( ! wtf ) {
		DBG ( "[NULL ComponentName]" );
		return NULL;
	}

	/* Try "eng" first; if that fails then try the first language */
	if ( ( ( efirc = wtf->GetDriverName ( wtf, "eng",
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
 * Get driver name
 *
 * @v wtf		Component name protocol
 * @ret name		Driver name, or NULL
 */
static const char * efi_driver_name2 ( EFI_COMPONENT_NAME2_PROTOCOL *wtf ) {
	static char name[64];
	CHAR16 *driver_name;
	EFI_STATUS efirc;

	/* Sanity check */
	if ( ! wtf ) {
		DBG ( "[NULL ComponentName2]" );
		return NULL;
	}

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
 * Get driver binding name
 *
 * @v binding		Driver binding protocol
 * @ret name		Driver name, or NULL
 */
static const char * efi_binding_name ( EFI_DRIVER_BINDING_PROTOCOL *binding ) {
	EFI_COMPONENT_NAME_PROTOCOL *name;
	EFI_HANDLE image;
	int rc;

	/* Sanity check */
	if ( ! binding ) {
		DBG ( "[NULL DriverBinding]" );
		return NULL;
	}

	/* Try to open component name protocol on image handle */
	image = binding->ImageHandle;
	if ( ( rc = efi_open ( image, &efi_component_name_protocol_guid,
			       &name ) ) != 0 ) {
		DBG ( "[DriverBinding no ComponentName]" );
		return NULL;
	}

	/* Try to get name from component name protocol */
	return efi_driver_name ( name );
}

/**
 * Get driver binding name
 *
 * @v binding		Driver binding protocol
 * @ret name		Driver name, or NULL
 */
static const char * efi_binding_name2 ( EFI_DRIVER_BINDING_PROTOCOL *binding ){
	EFI_HANDLE image;
	EFI_COMPONENT_NAME2_PROTOCOL *name2;
	int rc;

	/* Sanity check */
	if ( ! binding ) {
		DBG ( "[NULL DriverBinding]" );
		return NULL;
	}

	/* Try to open component name protocol on image handle */
	image = binding->ImageHandle;
	if ( ( rc = efi_open ( image, &efi_component_name2_protocol_guid,
			       &name2 ) ) != 0 ) {
		DBG ( "[DriverBinding no ComponentName2]" );
		return NULL;
	}

	/* Try to get name from component name protocol */
	return efi_driver_name2 ( name2 );
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
	EFI_IMAGE_DOS_HEADER *dos;
	EFI_IMAGE_OPTIONAL_HEADER_UNION *pe;
	EFI_IMAGE_OPTIONAL_HEADER32 *opt32;
	EFI_IMAGE_OPTIONAL_HEADER64 *opt64;
	EFI_IMAGE_DATA_DIRECTORY *datadir;
	EFI_IMAGE_DEBUG_DIRECTORY_ENTRY *debug;
	EFI_IMAGE_DEBUG_CODEVIEW_NB10_ENTRY *codeview_nb10;
	EFI_IMAGE_DEBUG_CODEVIEW_RSDS_ENTRY *codeview_rsds;
	EFI_IMAGE_DEBUG_CODEVIEW_MTOC_ENTRY *codeview_mtoc;
	uint16_t dos_magic;
	uint32_t pe_magic;
	uint16_t opt_magic;
	uint32_t codeview_magic;
	size_t max_len;
	char *name;
	char *tmp;

	/* Sanity check */
	if ( ! loaded ) {
		DBG ( "[NULL LoadedImage]" );
		return NULL;
	}

	/* Parse DOS header */
	dos = loaded->ImageBase;
	if ( ! dos ) {
		DBG ( "[Missing DOS header]" );
		return NULL;
	}
	dos_magic = dos->e_magic;
	if ( dos_magic != EFI_IMAGE_DOS_SIGNATURE ) {
		DBG ( "[Bad DOS signature %#04x]", dos_magic );
		return NULL;
	}
	pe = ( loaded->ImageBase + dos->e_lfanew );

	/* Parse PE header */
	pe_magic = pe->Pe32.Signature;
	if ( pe_magic != EFI_IMAGE_NT_SIGNATURE ) {
		DBG ( "[Bad PE signature %#08x]", pe_magic );
		return NULL;
	}
	opt32 = &pe->Pe32.OptionalHeader;
	opt64 = &pe->Pe32Plus.OptionalHeader;
	opt_magic = opt32->Magic;
	if ( opt_magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC ) {
		datadir = opt32->DataDirectory;
	} else if ( opt_magic == EFI_IMAGE_NT_OPTIONAL_HDR64_MAGIC ) {
		datadir = opt64->DataDirectory;
	} else {
		DBG ( "[Bad optional header signature %#04x]", opt_magic );
		return NULL;
	}

	/* Parse data directory entry */
	if ( ! datadir[EFI_IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress ) {
		DBG ( "[Empty debug directory entry]" );
		return NULL;
	}
	debug = ( loaded->ImageBase +
		  datadir[EFI_IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress );

	/* Parse debug directory entry */
	if ( debug->Type != EFI_IMAGE_DEBUG_TYPE_CODEVIEW ) {
		DBG ( "[Not a CodeView debug directory entry (type %d)]",
		      debug->Type );
		return NULL;
	}
	codeview_nb10 = ( loaded->ImageBase + debug->RVA );
	codeview_rsds = ( loaded->ImageBase + debug->RVA );
	codeview_mtoc = ( loaded->ImageBase + debug->RVA );
	codeview_magic = codeview_nb10->Signature;

	/* Parse CodeView entry */
	if ( codeview_magic == CODEVIEW_SIGNATURE_NB10 ) {
		name = ( ( void * ) ( codeview_nb10 + 1 ) );
	} else if ( codeview_magic == CODEVIEW_SIGNATURE_RSDS ) {
		name = ( ( void * ) ( codeview_rsds + 1 ) );
	} else if ( codeview_magic == CODEVIEW_SIGNATURE_MTOC ) {
		name = ( ( void * ) ( codeview_mtoc + 1 ) );
	} else {
		DBG ( "[Bad CodeView signature %#08x]", codeview_magic );
		return NULL;
	}

	/* Sanity check - avoid scanning endlessly through memory */
	max_len = EFI_PAGE_SIZE; /* Reasonably sane */
	if ( strnlen ( name, max_len ) == max_len ) {
		DBG ( "[Excessively long or invalid CodeView name]" );
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
	if ( ( tmp = strrchr ( buf, '.' ) ) != NULL )
		*tmp = '\0';

	return buf;
}

/**
 * Get initial loaded image name
 *
 * @v loaded		Loaded image
 * @ret name		Initial loaded image name, or NULL
 */
static const char *
efi_first_loaded_image_name ( EFI_LOADED_IMAGE_PROTOCOL *loaded ) {

	/* Sanity check */
	if ( ! loaded ) {
		DBG ( "[NULL LoadedImage]" );
		return NULL;
	}

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

	/* Sanity check */
	if ( ! loaded ) {
		DBG ( "[NULL LoadedImage]" );
		return NULL;
	}

	return efi_devpath_text ( loaded->FilePath );
}

/**
 * Get console input handle name
 *
 * @v input		Simple text input protocol
 * @ret name		Console input handle name, or NULL
 */
static const char *
efi_conin_name ( EFI_SIMPLE_TEXT_INPUT_PROTOCOL *input ) {

	/* Check for match against ConIn */
	if ( input == efi_systab->ConIn )
		return "ConIn";

	return NULL;
}

/**
 * Get console output handle name
 *
 * @v output		Simple text output protocol
 * @ret name		Console output handle name, or NULL
 */
static const char *
efi_conout_name ( EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *output ) {

	/* Check for match against ConOut */
	if ( output == efi_systab->ConOut )
		return "ConOut";

	/* Check for match against StdErr (if different from ConOut) */
	if ( output == efi_systab->StdErr )
		return "StdErr";

	return NULL;
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
	/* Driver name (for driver binding handles) */
	EFI_HANDLE_NAME_TYPE ( &efi_driver_binding_protocol_guid,
			       efi_binding_name2 ),
	/* Driver name (via obsolete original ComponentName protocol) */
	EFI_HANDLE_NAME_TYPE ( &efi_driver_binding_protocol_guid,
			       efi_binding_name ),
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
	/* Device path */
	EFI_HANDLE_NAME_TYPE ( &efi_device_path_protocol_guid,
			       efi_devpath_text ),
	/* Our standard input file handle */
	EFI_HANDLE_NAME_TYPE ( &efi_simple_text_input_protocol_guid,
			       efi_conin_name ),
	/* Our standard output and standard error file handles */
	EFI_HANDLE_NAME_TYPE ( &efi_simple_text_output_protocol_guid,
			       efi_conout_name ),
};

/**
 * Get name of an EFI handle
 *
 * @v handle		EFI handle
 * @ret text		Name of handle, or NULL
 */
const __attribute__ (( pure )) char * efi_handle_name ( EFI_HANDLE handle ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct efi_handle_name_type *type;
	static char buf[256];
	size_t used = 0;
	EFI_GUID **protocols;
	UINTN count;
	unsigned int i;
	void *interface;
	const char *name;
	EFI_STATUS efirc;
	int rc;

	/* Fail immediately for NULL handles */
	if ( ! handle )
		return NULL;

	/* Try each name type in turn */
	for ( i = 0 ; i < ( sizeof ( efi_handle_name_types ) /
			    sizeof ( efi_handle_name_types[0] ) ) ; i++ ) {
		type = &efi_handle_name_types[i];
		DBG2 ( "<%d", i );

		/* Try to open the applicable protocol */
		if ( ( rc = efi_open ( handle, type->protocol,
				       &interface ) ) != 0 ) {
			DBG2 ( ">" );
			continue;
		}

		/* Try to get name from this protocol */
		DBG2 ( "-" );
		name = type->name ( interface );
		DBG2 ( "%c>", ( name ? ( name[0] ? 'Y' : 'E' ) : 'N' ) );

		/* Use this name, if possible */
		if ( name && name[0] )
			return name;
	}

	/* If no name is found, then use the raw handle value and a
	 * list of installed protocols.
	 */
	used = ssnprintf ( buf, sizeof ( buf ), "UNKNOWN<%p", handle );
	if ( ( efirc = bs->ProtocolsPerHandle ( handle, &protocols,
						&count ) ) == 0 ) {
		for ( i = 0 ; i < count ; i++ ) {
			used += ssnprintf ( ( buf + used ),
					    ( sizeof ( buf ) - used ), ",%s",
					    efi_guid_ntoa ( protocols[i] ) );
		}
		bs->FreePool ( protocols );
	}
	used += ssnprintf ( ( buf + used ), ( sizeof ( buf ) - used ), ">" );
	return buf;
}
