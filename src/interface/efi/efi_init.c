/*
 * Copyright (C) 2008 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <ipxe/init.h>
#include <ipxe/settings.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_utils.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/Protocol/LoadedImage.h>

/** Image handle passed to entry point */
EFI_HANDLE efi_image_handle;

/** Loaded image protocol for this image */
EFI_LOADED_IMAGE_PROTOCOL *efi_loaded_image;

/** System table passed to entry point
 *
 * We construct the symbol name efi_systab via the PLATFORM macro.
 * This ensures that the symbol is defined only in EFI builds, and so
 * prevents EFI code from being incorrectly linked in to a non-EFI
 * build.
 */
EFI_SYSTEM_TABLE * _C2 ( PLATFORM, _systab );

/** EFI shutdown is in progress */
int efi_shutdown_in_progress;

/** Event used to signal shutdown */
static EFI_EVENT efi_shutdown_event;

/** EFI loaded image VLAN **/
static UINT16 efi_boot_vlan = 0;

/* Forward declarations */
static EFI_STATUS EFIAPI efi_unload ( EFI_HANDLE image_handle );

/**
 * Shut down in preparation for booting an OS.
 *
 * This hook gets called at ExitBootServices time in order to make
 * sure that everything is properly shut down before the OS takes
 * over.
 */
static EFIAPI void efi_shutdown_hook ( EFI_EVENT event __unused,
				       void *context __unused ) {

	/* Mark shutdown as being in progress, to indicate that large
	 * parts of the system (e.g. timers) are no longer functional.
	 */
	efi_shutdown_in_progress = 1;

	/* Shut down iPXE */
	shutdown_boot();
}

/**
 * Look up EFI configuration table
 *
 * @v guid		Configuration table GUID
 * @ret table		Configuration table, or NULL
 */
static void * efi_find_table ( EFI_GUID *guid ) {
	unsigned int i;

	for ( i = 0 ; i < efi_systab->NumberOfTableEntries ; i++ ) {
		if ( memcmp ( &efi_systab->ConfigurationTable[i].VendorGuid,
			      guid, sizeof ( *guid ) ) == 0 )
			return efi_systab->ConfigurationTable[i].VendorTable;
	}

	return NULL;
}

/**
 * Retrieve loaded image VLAN
 *
 * @ret efirc		EFI return status code
 */
static EFI_STATUS efi_get_boot_vlan ( EFI_DEVICE_PATH_PROTOCOL *device_path,
				      UINT16 *vlan ) {
	EFI_STATUS efirc;
	VLAN_DEVICE_PATH *vlan_path;

	if ( ( efirc = efi_devpath_find_node (
		device_path,
		MESSAGING_DEVICE_PATH, MSG_VLAN_DP,
		( EFI_DEVICE_PATH_PROTOCOL ** ) &vlan_path ) ) != 0 ) {
		return efirc;
	}

	*vlan = vlan_path->VlanId;

	DBGC2 ( device_path, "EFI found boot-vlan: %d\n", vlan_path->VlanId );

	return EFI_SUCCESS;
}

/**
 * Initialise EFI environment
 *
 * @v image_handle	Image handle
 * @v systab		System table
 * @ret efirc		EFI return status code
 */
EFI_STATUS efi_init ( EFI_HANDLE image_handle,
		      EFI_SYSTEM_TABLE *systab ) {
	EFI_BOOT_SERVICES *bs;
	struct efi_protocol *prot;
	struct efi_config_table *tab;
	void *loaded_image;
	EFI_DEVICE_PATH_PROTOCOL *device_path;
	EFI_STATUS efirc;
	int rc;

	/* Store image handle and system table pointer for future use */
	efi_image_handle = image_handle;
	efi_systab = systab;

	/* Sanity checks */
	if ( ! systab ) {
		efirc = EFI_NOT_AVAILABLE_YET;
		goto err_sanity;
	}
	if ( ! systab->ConOut ) {
		efirc = EFI_NOT_AVAILABLE_YET;
		goto err_sanity;
	}
	if ( ! systab->BootServices ) {
		DBGC ( systab, "EFI provided no BootServices entry point\n" );
		efirc = EFI_NOT_AVAILABLE_YET;
		goto err_sanity;
	}
	if ( ! systab->RuntimeServices ) {
		DBGC ( systab, "EFI provided no RuntimeServices entry "
		       "point\n" );
		efirc = EFI_NOT_AVAILABLE_YET;
		goto err_sanity;
	}
	DBGC ( systab, "EFI handle %p systab %p\n", image_handle, systab );
	bs = systab->BootServices;

	/* Look up used protocols */
	for_each_table_entry ( prot, EFI_PROTOCOLS ) {
		if ( ( efirc = bs->LocateProtocol ( &prot->guid, NULL,
						    prot->protocol ) ) == 0 ) {
			DBGC ( systab, "EFI protocol %s is at %p\n",
			       efi_guid_ntoa ( &prot->guid ),
			       *(prot->protocol) );
		} else {
			DBGC ( systab, "EFI does not provide protocol %s\n",
			       efi_guid_ntoa ( &prot->guid ) );
			/* Fail if protocol is required */
			if ( prot->required )
				goto err_missing_protocol;
		}
	}

	/* Look up used configuration tables */
	for_each_table_entry ( tab, EFI_CONFIG_TABLES ) {
		if ( ( *(tab->table) = efi_find_table ( &tab->guid ) ) ) {
			DBGC ( systab, "EFI configuration table %s is at %p\n",
			       efi_guid_ntoa ( &tab->guid ), *(tab->table) );
		} else {
			DBGC ( systab, "EFI does not provide configuration "
			       "table %s\n", efi_guid_ntoa ( &tab->guid ) );
			if ( tab->required ) {
				efirc = EFI_NOT_AVAILABLE_YET;
				goto err_missing_table;
			}
		}
	}

	/* Get loaded image protocol */
	if ( ( efirc = bs->OpenProtocol ( image_handle,
				&efi_loaded_image_protocol_guid,
				&loaded_image, image_handle, NULL,
				EFI_OPEN_PROTOCOL_GET_PROTOCOL ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( systab, "EFI could not get loaded image protocol: %s",
		       strerror ( rc ) );
		goto err_no_loaded_image;
	}
	efi_loaded_image = loaded_image;
	DBGC ( systab, "EFI image base address %p\n",
	       efi_loaded_image->ImageBase );

	/* Get loaded image device path */
	if ( ( efirc = bs->HandleProtocol (
		efi_loaded_image->DeviceHandle,
		&efi_device_path_protocol_guid,
		( void ** ) &device_path ) ) != 0 ) {

		rc = -EEFI ( efirc );
		DBGC ( systab, "EFI failed to retrieve image device path: %s",
		       strerror ( rc ) );
		goto err_no_device_path;
	}

	DBGC ( systab, "EFI image device path: %s\n",
	       efi_devpath_text ( device_path ) );

	/* Get loaded image VLAN*/
	if ( ( efirc = efi_get_boot_vlan ( device_path,
					   &efi_boot_vlan ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( systab, "EFI failed to retrieve loaded image VLAN: %s\n",
		       strerror ( rc ) );
	}

	/* EFI is perfectly capable of gracefully shutting down any
	 * loaded devices if it decides to fall back to a legacy boot.
	 * For no particularly comprehensible reason, it doesn't
	 * bother doing so when ExitBootServices() is called.
	 */
	if ( ( efirc = bs->CreateEvent ( EVT_SIGNAL_EXIT_BOOT_SERVICES,
					 TPL_CALLBACK, efi_shutdown_hook,
					 NULL, &efi_shutdown_event ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( systab, "EFI could not create ExitBootServices event: "
		       "%s\n", strerror ( rc ) );
		goto err_create_event;
	}

	/* Install driver binding protocol */
	if ( ( rc = efi_driver_install() ) != 0 ) {
		DBGC ( systab, "EFI could not install driver: %s\n",
		       strerror ( rc ) );
		efirc = EFIRC ( rc );
		goto err_driver_install;
	}

	/* Install image unload method */
	efi_loaded_image->Unload = efi_unload;

	return 0;

	efi_driver_uninstall();
 err_driver_install:
	bs->CloseEvent ( efi_shutdown_event );
 err_create_event:
 err_no_loaded_image:
 err_no_device_path:
 err_missing_table:
 err_missing_protocol:
 err_sanity:
	return efirc;
}

/**
 * Shut down EFI environment
 *
 * @v image_handle	Image handle
 */
static EFI_STATUS EFIAPI efi_unload ( EFI_HANDLE image_handle __unused ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_SYSTEM_TABLE *systab = efi_systab;

	DBGC ( systab, "EFI image unloading\n" );

	/* Shut down */
	shutdown_exit();

	/* Disconnect any remaining devices */
	efi_driver_disconnect_all();

	/* Uninstall driver binding protocol */
	efi_driver_uninstall();

	/* Uninstall exit boot services event */
	bs->CloseEvent ( efi_shutdown_event );

	DBGC ( systab, "EFI image unloaded\n" );

	return 0;
}

/**
 * Fetch boot VLAN setting
 *
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int efi_boot_vlan_fetch ( void *data, size_t len ) {
	uint16_t content;

	content = htons ( efi_boot_vlan );
	if ( len > sizeof ( content ) )
		len = sizeof ( content );
	memcpy ( data, &content, len );
	return sizeof ( content );
}

/** EFI boot VLAN setting */
const struct setting efi_boot_vlan_setting __setting ( SETTING_BOOT, boot-vlan ) = {
	.name = "boot-vlan",
	.description = "EFI boot VLAN",
	.type = &setting_type_int16,
	.scope = &builtin_scope,
};

/** EFI boot VLAN built-in setting */
struct builtin_setting efi_boot_vlan_builtin_setting __builtin_setting = {
	.setting = &efi_boot_vlan_setting,
	.fetch = efi_boot_vlan_fetch,
};