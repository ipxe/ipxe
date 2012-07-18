/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <stdlib.h>
#include <errno.h>
#include <ipxe/device.h>
#include <ipxe/netdevice.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_hii.h>
#include <ipxe/efi/efi_snp.h>
#include <ipxe/efi/efi_strings.h>
#include <config/general.h>

/** EFI configuration access protocol GUID */
static EFI_GUID efi_hii_config_access_protocol_guid
	= EFI_HII_CONFIG_ACCESS_PROTOCOL_GUID;

/** EFI HII database protocol */
static EFI_HII_DATABASE_PROTOCOL *efihii;
EFI_REQUIRE_PROTOCOL ( EFI_HII_DATABASE_PROTOCOL, &efihii );

/** Local base GUID used for our EFI SNP formset */
#define EFI_SNP_FORMSET_GUID_BASE					\
	{ 0xc4f84019, 0x6dfd, 0x4a27,					\
	  { 0x9b, 0x94, 0xb7, 0x2e, 0x1f, 0xbc, 0xad, 0xca } }

/** Form identifiers used for our EFI SNP HII */
enum efi_snp_hii_form_id {
	EFI_SNP_FORM = 0x0001,		/**< The only form */
};

/** String identifiers used for our EFI SNP HII */
enum efi_snp_hii_string_id {
	/* Language name */
	EFI_SNP_LANGUAGE_NAME = 0x0001,
	/* Formset */
	EFI_SNP_FORMSET_TITLE, EFI_SNP_FORMSET_HELP,
	/* Product name */
	EFI_SNP_PRODUCT_PROMPT, EFI_SNP_PRODUCT_HELP, EFI_SNP_PRODUCT_TEXT,
	/* Version */
	EFI_SNP_VERSION_PROMPT, EFI_SNP_VERSION_HELP, EFI_SNP_VERSION_TEXT,
	/* Driver */
	EFI_SNP_DRIVER_PROMPT, EFI_SNP_DRIVER_HELP, EFI_SNP_DRIVER_TEXT,
	/* Device */
	EFI_SNP_DEVICE_PROMPT, EFI_SNP_DEVICE_HELP, EFI_SNP_DEVICE_TEXT,
	/* End of list */
	EFI_SNP_MAX_STRING_ID
};

/** EFI SNP formset */
struct efi_snp_formset {
	EFI_HII_PACKAGE_HEADER Header;
	EFI_IFR_FORM_SET_TYPE(2) FormSet;
	EFI_IFR_GUID_CLASS Class;
	EFI_IFR_GUID_SUBCLASS SubClass;
	EFI_IFR_FORM Form;
	EFI_IFR_TEXT ProductText;
	EFI_IFR_TEXT VersionText;
	EFI_IFR_TEXT DriverText;
	EFI_IFR_TEXT DeviceText;
	EFI_IFR_END EndForm;
	EFI_IFR_END EndFormSet;
} __attribute__ (( packed )) efi_snp_formset = {
	.Header = {
		.Length = sizeof ( efi_snp_formset ),
		.Type = EFI_HII_PACKAGE_FORMS,
	},
	.FormSet = EFI_IFR_FORM_SET ( EFI_SNP_FORMSET_GUID_BASE,
				      EFI_SNP_FORMSET_TITLE,
				      EFI_SNP_FORMSET_HELP,
				      typeof ( efi_snp_formset.FormSet ),
				      EFI_HII_PLATFORM_SETUP_FORMSET_GUID,
				      EFI_HII_IBM_UCM_COMPLIANT_FORMSET_GUID ),
	.Class = EFI_IFR_GUID_CLASS ( EFI_NETWORK_DEVICE_CLASS ),
	.SubClass = EFI_IFR_GUID_SUBCLASS ( 0x03 ),
	.Form = EFI_IFR_FORM ( EFI_SNP_FORM, EFI_SNP_FORMSET_TITLE ),
	.ProductText = EFI_IFR_TEXT ( EFI_SNP_PRODUCT_PROMPT,
				      EFI_SNP_PRODUCT_HELP,
				      EFI_SNP_PRODUCT_TEXT ),
	.VersionText = EFI_IFR_TEXT ( EFI_SNP_VERSION_PROMPT,
				      EFI_SNP_VERSION_HELP,
				      EFI_SNP_VERSION_TEXT ),
	.DriverText = EFI_IFR_TEXT ( EFI_SNP_DRIVER_PROMPT,
				     EFI_SNP_DRIVER_HELP,
				     EFI_SNP_DRIVER_TEXT ),
	.DeviceText = EFI_IFR_TEXT ( EFI_SNP_DEVICE_PROMPT,
				     EFI_SNP_DEVICE_HELP,
				     EFI_SNP_DEVICE_TEXT ),
	.EndForm = EFI_IFR_END(),
	.EndFormSet = EFI_IFR_END(),
};

/**
 * Generate EFI SNP string
 *
 * @v wbuf		Buffer
 * @v swlen		Size of buffer (in wide characters)
 * @v snpdev		SNP device
 * @ret wlen		Length of string (in wide characters)
 */
static int efi_snp_string ( wchar_t *wbuf, ssize_t swlen,
			    enum efi_snp_hii_string_id id,
			    struct efi_snp_device *snpdev ) {
	struct net_device *netdev = snpdev->netdev;
	struct device *dev = netdev->dev;

	switch ( id ) {
	case EFI_SNP_LANGUAGE_NAME:
		return efi_ssnprintf ( wbuf, swlen, "English" );
	case EFI_SNP_FORMSET_TITLE:
		return efi_ssnprintf ( wbuf, swlen, "%s (%s)",
				       ( PRODUCT_NAME[0] ?
					 PRODUCT_NAME : PRODUCT_SHORT_NAME ),
				       netdev_addr ( netdev ) );
	case EFI_SNP_FORMSET_HELP:
		return efi_ssnprintf ( wbuf, swlen,
				       "Configure " PRODUCT_SHORT_NAME );
	case EFI_SNP_PRODUCT_PROMPT:
		return efi_ssnprintf ( wbuf, swlen, "Name" );
	case EFI_SNP_PRODUCT_HELP:
		return efi_ssnprintf ( wbuf, swlen, "Firmware product name" );
	case EFI_SNP_PRODUCT_TEXT:
		return efi_ssnprintf ( wbuf, swlen, "%s",
				       ( PRODUCT_NAME[0] ?
					 PRODUCT_NAME : PRODUCT_SHORT_NAME ) );
	case EFI_SNP_VERSION_PROMPT:
		return efi_ssnprintf ( wbuf, swlen, "Version" );
	case EFI_SNP_VERSION_HELP:
		return efi_ssnprintf ( wbuf, swlen, "Firmware version" );
	case EFI_SNP_VERSION_TEXT:
		return efi_ssnprintf ( wbuf, swlen, VERSION );
	case EFI_SNP_DRIVER_PROMPT:
		return efi_ssnprintf ( wbuf, swlen, "Driver" );
	case EFI_SNP_DRIVER_HELP:
		return efi_ssnprintf ( wbuf, swlen, "Firmware driver" );
	case EFI_SNP_DRIVER_TEXT:
		return efi_ssnprintf ( wbuf, swlen, "%s", dev->driver_name );
	case EFI_SNP_DEVICE_PROMPT:
		return efi_ssnprintf ( wbuf, swlen, "Device" );
	case EFI_SNP_DEVICE_HELP:
		return efi_ssnprintf ( wbuf, swlen, "Hardware device" );
	case EFI_SNP_DEVICE_TEXT:
		return efi_ssnprintf ( wbuf, swlen, "%s", dev->name );
	default:
		assert ( 0 );
		return 0;
	}
}

/**
 * Generate EFI SNP string package
 *
 * @v strings		String package header buffer
 * @v max_len		Buffer length
 * @v snpdev		SNP device
 * @ret len		Length of string package
 */
static int efi_snp_strings ( EFI_HII_STRING_PACKAGE_HDR *strings,
			     size_t max_len, struct efi_snp_device *snpdev ) {
	static const char language[] = "en-us";
	void *buf = strings;
	ssize_t remaining = max_len;
	size_t hdrsize;
	EFI_HII_SIBT_STRING_UCS2_BLOCK *string;
	ssize_t wremaining;
	size_t string_wlen;
	unsigned int id;
	EFI_HII_STRING_BLOCK *end;
	size_t len;

	/* Calculate header size */
	hdrsize = ( offsetof ( typeof ( *strings ), Language ) +
		    sizeof ( language ) );
	buf += hdrsize;
	remaining -= hdrsize;

	/* Fill in strings */
	for ( id = 1 ; id < EFI_SNP_MAX_STRING_ID ; id++ ) {
		string = buf;
		if ( remaining >= ( ( ssize_t ) sizeof ( string->Header ) ) )
			string->Header.BlockType = EFI_HII_SIBT_STRING_UCS2;
		buf += offsetof ( typeof ( *string ), StringText );
		remaining -= offsetof ( typeof ( *string ), StringText );
		wremaining = ( remaining /
			       ( ( ssize_t ) sizeof ( string->StringText[0] )));
		assert ( ! ( ( remaining <= 0 ) && ( wremaining > 0 ) ) );
		string_wlen = efi_snp_string ( string->StringText, wremaining,
					       id, snpdev );
		buf += ( ( string_wlen + 1 /* wNUL */ ) *
			 sizeof ( string->StringText[0] ) );
		remaining -= ( ( string_wlen + 1 /* wNUL */ ) *
			       sizeof ( string->StringText[0] ) );
	}

	/* Fill in end marker */
	end = buf;
	if ( remaining >= ( ( ssize_t ) sizeof ( *end ) ) )
		end->BlockType = EFI_HII_SIBT_END;
	buf += sizeof ( *end );
	remaining -= sizeof ( *end );

	/* Calculate overall length */
	len = ( max_len - remaining );

	/* Fill in string package header */
	if ( strings ) {
		memset ( strings, 0, sizeof ( *strings ) );
		strings->Header.Length = len;
		strings->Header.Type = EFI_HII_PACKAGE_STRINGS;
		strings->HdrSize = hdrsize;
		strings->StringInfoOffset = hdrsize;
		strings->LanguageName = EFI_SNP_LANGUAGE_NAME;
		memcpy ( strings->Language, language, sizeof ( language ) );
	}

	return len;
}

/**
 * Generate EFI SNP package list
 *
 * @v snpdev		SNP device
 * @ret package_list	Package list, or NULL on error
 *
 * The package list is allocated using malloc(), and must eventually
 * be freed by the caller.
 */
static EFI_HII_PACKAGE_LIST_HEADER *
efi_snp_package_list ( struct efi_snp_device *snpdev ) {
	size_t strings_len = efi_snp_strings ( NULL, 0, snpdev );
	struct {
		EFI_HII_PACKAGE_LIST_HEADER header;
		struct efi_snp_formset formset;
		union {
			EFI_HII_STRING_PACKAGE_HDR strings;
			uint8_t pad[strings_len];
		} __attribute__ (( packed )) strings;
		EFI_HII_PACKAGE_HEADER end;
	} __attribute__ (( packed )) *package_list;

	/* Allocate package list */
	package_list = zalloc ( sizeof ( *package_list ) );
	if ( ! package_list )
		return NULL;

	/* Create a unique GUID for this package list and formset */
	efi_snp_formset.FormSet.FormSet.Guid.Data1++;

	/* Populate package list */
	memcpy ( &package_list->header.PackageListGuid,
		 &efi_snp_formset.FormSet.FormSet.Guid,
		 sizeof ( package_list->header.PackageListGuid ) );
	package_list->header.PackageLength = sizeof ( *package_list );
	memcpy ( &package_list->formset, &efi_snp_formset,
		 sizeof ( package_list->formset ) );
	efi_snp_strings ( &package_list->strings.strings,
			  sizeof ( package_list->strings ), snpdev );
	package_list->end.Length = sizeof ( package_list->end );
	package_list->end.Type = EFI_HII_PACKAGE_END;

	return &package_list->header;
}

/**
 * Fetch configuration
 *
 * @v hii		HII configuration access protocol
 * @v request		Configuration to fetch
 * @ret progress	Progress made through configuration to fetch
 * @ret results		Query results
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_snp_hii_extract_config ( const EFI_HII_CONFIG_ACCESS_PROTOCOL *hii,
			     EFI_STRING request, EFI_STRING *progress,
			     EFI_STRING *results __unused ) {
	struct efi_snp_device *snpdev =
		container_of ( hii, struct efi_snp_device, hii );

	DBGC ( snpdev, "SNPDEV %p ExtractConfig \"%ls\"\n", snpdev, request );

	*progress = request;
	return EFI_INVALID_PARAMETER;
}

/**
 * Store configuration
 *
 * @v hii		HII configuration access protocol
 * @v config		Configuration to store
 * @ret progress	Progress made through configuration to store
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_snp_hii_route_config ( const EFI_HII_CONFIG_ACCESS_PROTOCOL *hii,
			   EFI_STRING config, EFI_STRING *progress ) {
	struct efi_snp_device *snpdev =
		container_of ( hii, struct efi_snp_device, hii );

	DBGC ( snpdev, "SNPDEV %p RouteConfig \"%ls\"\n", snpdev, config );

	*progress = config;
	return EFI_INVALID_PARAMETER;
}

/**
 * Handle form actions
 *
 * @v hii		HII configuration access protocol
 * @v action		Form browser action
 * @v question_id	Question ID
 * @v type		Type of value
 * @v value		Value
 * @ret action_request	Action requested by driver
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_snp_hii_callback ( const EFI_HII_CONFIG_ACCESS_PROTOCOL *hii,
		       EFI_BROWSER_ACTION action __unused,
		       EFI_QUESTION_ID question_id __unused,
		       UINT8 type __unused, EFI_IFR_TYPE_VALUE *value __unused,
		       EFI_BROWSER_ACTION_REQUEST *action_request __unused ) {
	struct efi_snp_device *snpdev =
		container_of ( hii, struct efi_snp_device, hii );

	DBGC ( snpdev, "SNPDEV %p Callback\n", snpdev );
	return EFI_UNSUPPORTED;
}

/** HII configuration access protocol */
static EFI_HII_CONFIG_ACCESS_PROTOCOL efi_snp_device_hii = {
	.ExtractConfig	= efi_snp_hii_extract_config,
	.RouteConfig	= efi_snp_hii_route_config,
	.Callback	= efi_snp_hii_callback,
};

/**
 * Install HII protocol and packages for SNP device
 *
 * @v snpdev		SNP device
 * @ret rc		Return status code
 */
int efi_snp_hii_install ( struct efi_snp_device *snpdev ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	int efirc;
	int rc;

	/* Initialise HII protocol */
	memcpy ( &snpdev->hii, &efi_snp_device_hii, sizeof ( snpdev->hii ) );

	/* Create HII package list */
	snpdev->package_list = efi_snp_package_list ( snpdev );
	if ( ! snpdev->package_list ) {
		DBGC ( snpdev, "SNPDEV %p could not create HII package list\n",
		       snpdev );
		rc = -ENOMEM;
		goto err_build_package_list;
	}

	/* Add HII packages */
	if ( ( efirc = efihii->NewPackageList ( efihii, snpdev->package_list,
						snpdev->handle,
						&snpdev->hii_handle ) ) != 0 ) {
		DBGC ( snpdev, "SNPDEV %p could not add HII packages: %s\n",
		       snpdev, efi_strerror ( efirc ) );
		rc = EFIRC_TO_RC ( efirc );
		goto err_new_package_list;
	}

	/* Install HII protocol */
	if ( ( efirc = bs->InstallMultipleProtocolInterfaces (
			 &snpdev->handle,
			 &efi_hii_config_access_protocol_guid, &snpdev->hii,
			 NULL ) ) != 0 ) {
		DBGC ( snpdev, "SNPDEV %p could not install HII protocol: %s\n",
		       snpdev, efi_strerror ( efirc ) );
		rc = EFIRC_TO_RC ( efirc );
		goto err_install_protocol;
	}

	return 0;

	bs->UninstallMultipleProtocolInterfaces (
			snpdev->handle,
			&efi_hii_config_access_protocol_guid, &snpdev->hii,
			NULL );
 err_install_protocol:
	efihii->RemovePackageList ( efihii, snpdev->hii_handle );
 err_new_package_list:
	free ( snpdev->package_list );
	snpdev->package_list = NULL;
 err_build_package_list:
	return rc;
}

/**
 * Uninstall HII protocol and package for SNP device
 *
 * @v snpdev		SNP device
 */
void efi_snp_hii_uninstall ( struct efi_snp_device *snpdev ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;

	bs->UninstallMultipleProtocolInterfaces (
			snpdev->handle,
			&efi_hii_config_access_protocol_guid, &snpdev->hii,
			NULL );
	efihii->RemovePackageList ( efihii, snpdev->hii_handle );
	free ( snpdev->package_list );
	snpdev->package_list = NULL;
}
