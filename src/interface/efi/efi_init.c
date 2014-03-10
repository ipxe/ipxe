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
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/Protocol/LoadedImage.h>
#include <ipxe/efi/Protocol/DevicePath.h>
#include <ipxe/uuid.h>
#include <ipxe/init.h>

/** Image handle passed to entry point */
EFI_HANDLE efi_image_handle;

/** Loaded image protocol for this image */
EFI_LOADED_IMAGE_PROTOCOL *efi_loaded_image;

/** Loaded image protocol device path for this image */
EFI_DEVICE_PATH_PROTOCOL *efi_loaded_image_path;

/** System table passed to entry point */
EFI_SYSTEM_TABLE *efi_systab;

/** EFI loaded image protocol GUID */
static EFI_GUID efi_loaded_image_protocol_guid
	= EFI_LOADED_IMAGE_PROTOCOL_GUID;

/** EFI loaded image device path protocol GUID */
static EFI_GUID efi_loaded_image_device_path_protocol_guid
	= EFI_LOADED_IMAGE_DEVICE_PATH_PROTOCOL_GUID;

/** Event used to signal shutdown */
static EFI_EVENT efi_shutdown_event;

/* Forward declarations */
static EFI_STATUS EFIAPI efi_unload ( EFI_HANDLE image_handle );

/**
 * Check to see if driver supports a device
 *
 * @v driver		EFI driver
 * @v device		EFI device
 * @v child		Path to child device, if any
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_image_supported ( EFI_DRIVER_BINDING_PROTOCOL *driver __unused,
		    EFI_HANDLE device __unused,
		    EFI_DEVICE_PATH_PROTOCOL *child __unused ) {

	return EFI_UNSUPPORTED;
}

/**
 * Attach driver to device
 *
 * @v driver		EFI driver
 * @v device		EFI device
 * @v child		Path to child device, if any
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_image_start ( EFI_DRIVER_BINDING_PROTOCOL *driver __unused,
		EFI_HANDLE device __unused,
		EFI_DEVICE_PATH_PROTOCOL *child __unused ) {

	return EFI_UNSUPPORTED;
}

/**
 * Detach driver from device
 *
 * @v driver		EFI driver
 * @v device		EFI device
 * @v pci		PCI device
 * @v num_children	Number of child devices
 * @v children		List of child devices
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_image_stop ( EFI_DRIVER_BINDING_PROTOCOL *driver __unused,
	       EFI_HANDLE device __unused, UINTN num_children __unused,
	       EFI_HANDLE *children __unused ) {

	return EFI_UNSUPPORTED;
}

/** EFI loaded image driver */
static struct efi_driver efi_image_driver =
	EFI_DRIVER_INIT ( NULL, efi_image_supported, efi_image_start,
			  efi_image_stop );

/**
 * Shut down in preparation for booting an OS.
 *
 * This hook gets called at ExitBootServices time in order to make
 * sure that everything is properly shut down before the OS takes
 * over.
 */
static EFIAPI void efi_shutdown_hook ( EFI_EVENT event __unused,
				       void *context __unused ) {
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
	void *loaded_image_path;
	EFI_STATUS efirc;
	int rc;

	/* Store image handle and system table pointer for future use */
	efi_image_handle = image_handle;
	efi_systab = systab;

	/* Sanity checks */
	if ( ! systab )
		return EFI_NOT_AVAILABLE_YET;
	if ( ! systab->ConOut )
		return EFI_NOT_AVAILABLE_YET;
	if ( ! systab->BootServices ) {
		DBGC ( systab, "EFI provided no BootServices entry point\n" );
		return EFI_NOT_AVAILABLE_YET;
	}
	if ( ! systab->RuntimeServices ) {
		DBGC ( systab, "EFI provided no RuntimeServices entry "
		       "point\n" );
		return EFI_NOT_AVAILABLE_YET;
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
			/* All protocols are required */
			return efirc;
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
			if ( tab->required )
				return EFI_NOT_AVAILABLE_YET;
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
		return efirc;
	}
	efi_loaded_image = loaded_image;
	DBGC ( systab, "EFI image base address %p\n",
	       efi_loaded_image->ImageBase );

	/* Get loaded image device path protocol */
	if ( ( efirc = bs->OpenProtocol ( image_handle,
				&efi_loaded_image_device_path_protocol_guid,
				&loaded_image_path, image_handle, NULL,
				EFI_OPEN_PROTOCOL_GET_PROTOCOL ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( systab, "EFI could not get loaded image device path "
		       "protocol: %s", strerror ( rc ) );
		return efirc;
	}
	efi_loaded_image_path = loaded_image_path;
	DBGC ( systab, "EFI image device path " );
	DBGC_EFI_DEVPATH ( systab, efi_loaded_image_path );
	DBGC ( systab, "\n" );

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
		return efirc;
	}

	/* Install an EFI driver on the image handle, to allow the
	 * driver to be subsequently unloaded.
	 */
	efi_image_driver.driver.DriverBindingHandle = image_handle;
	if ( ( rc = efi_driver_install ( &efi_image_driver ) ) != 0 ) {
		DBGC ( systab, "EFI could not install loaded image driver: "
		       "%s\n", strerror ( rc ) );
		return EFIRC ( rc );
	}

	/* Install image unload method */
	efi_loaded_image->Unload = efi_unload;

	return 0;
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

	/* Uninstall exit boot services event */
	bs->CloseEvent ( efi_shutdown_event );

	/* Uninstall loaded image driver */
	efi_driver_uninstall ( &efi_image_driver );

	DBGC ( systab, "EFI image unloaded\n" );

	return 0;
}
