/*
 * Copyright (C) 2019 Michael Brown <mbrown@fensystems.co.uk>.
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

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <ipxe/settings.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/DriverBinding.h>
#include <ipxe/efi/Protocol/LoadedImage.h>
#include <ipxe/efi/Protocol/ComponentName.h>
#include <ipxe/efi/efi_veto.h>

/** @file
 *
 * EFI driver vetoes
 *
 */

/** A driver veto */
struct efi_veto {
	/** Veto name (for debugging) */
	const char *name;
	/**
	 * Check if driver is vetoed
	 *
	 * @v binding		Driver binding protocol
	 * @v loaded		Loaded image protocol
	 * @v wtf		Component name protocol, if present
	 * @ret vetoed		Driver is to be vetoed
	 */
	int ( * veto ) ( EFI_DRIVER_BINDING_PROTOCOL *binding,
			 EFI_LOADED_IMAGE_PROTOCOL *loaded,
			 EFI_COMPONENT_NAME_PROTOCOL *wtf );
};

/**
 * Veto Dell Ip4ConfigDxe driver
 *
 * @v binding		Driver binding protocol
 * @v loaded		Loaded image protocol
 * @v wtf		Component name protocol, if present
 * @ret vetoed		Driver is to be vetoed
 */
static int
efi_veto_dell_ip4config ( EFI_DRIVER_BINDING_PROTOCOL *binding __unused,
			  EFI_LOADED_IMAGE_PROTOCOL *loaded __unused,
			  EFI_COMPONENT_NAME_PROTOCOL *wtf ) {
	static const CHAR16 ip4cfg[] = L"IP4 CONFIG Network Service Driver";
	static const char dell[] = "Dell Inc.";
	char manufacturer[ sizeof ( dell ) ];
	CHAR16 *name;

	/* Check driver name */
	if ( ! wtf )
		return 0;
	if ( wtf->GetDriverName ( wtf, "eng", &name ) != 0 )
		return 0;
	if ( memcmp ( name, ip4cfg, sizeof ( ip4cfg ) ) != 0 )
		return 0;

	/* Check manufacturer */
	fetch_string_setting ( NULL, &manufacturer_setting, manufacturer,
			       sizeof ( manufacturer ) );
	if ( strcmp ( manufacturer, dell ) != 0 )
		return 0;

	return 1;
}

/** Driver vetoes */
static struct efi_veto efi_vetoes[] = {
	{
		.name = "Dell Ip4Config",
		.veto = efi_veto_dell_ip4config,
	},
};

/**
 * Find driver veto, if any
 *
 * @v driver		Driver binding handle
 * @ret veto		Driver veto, or NULL
 * @ret rc		Return status code
 */
static int efi_veto ( EFI_HANDLE driver, struct efi_veto **veto ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	union {
		EFI_DRIVER_BINDING_PROTOCOL *binding;
		void *interface;
	} binding;
	union {
		EFI_LOADED_IMAGE_PROTOCOL *loaded;
		void *interface;
	} loaded;
	union {
		EFI_COMPONENT_NAME_PROTOCOL *wtf;
		void *interface;
	} wtf;
	unsigned int i;
	EFI_HANDLE image;
	EFI_STATUS efirc;
	int rc;

	DBGC2 ( &efi_vetoes, "EFIVETO checking %s\n",
		efi_handle_name ( driver ) );

	/* Mark as not vetoed */
	*veto = NULL;

	/* Open driver binding protocol */
	if ( ( efirc = bs->OpenProtocol (
			driver, &efi_driver_binding_protocol_guid,
			&binding.interface, efi_image_handle, driver,
			EFI_OPEN_PROTOCOL_GET_PROTOCOL ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( driver, "EFIVETO %s could not open driver binding "
		       "protocol: %s\n", efi_handle_name ( driver ),
		       strerror ( rc ) );
		goto err_binding;
	}
	image = binding.binding->ImageHandle;

	/* Open loaded image protocol */
	if ( ( efirc = bs->OpenProtocol (
			image, &efi_loaded_image_protocol_guid,
			&loaded.interface, efi_image_handle, image,
			EFI_OPEN_PROTOCOL_GET_PROTOCOL ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( driver, "EFIVETO %s could not open",
		       efi_handle_name ( driver ) );
		DBGC ( driver, " %s loaded image protocol: %s\n",
		       efi_handle_name ( image ), strerror ( rc ) );
		goto err_loaded;
	}

	/* Open component name protocol, if present*/
	if ( ( efirc = bs->OpenProtocol (
			driver, &efi_component_name_protocol_guid,
			&wtf.interface, efi_image_handle, driver,
			EFI_OPEN_PROTOCOL_GET_PROTOCOL ) ) != 0 ) {
		/* Ignore failure; is not required to be present */
		wtf.interface = NULL;
	}

	/* Check vetoes */
	for ( i = 0 ; i < ( sizeof ( efi_vetoes ) /
			    sizeof ( efi_vetoes[0] ) ) ; i++ ) {
		if ( efi_vetoes[i].veto ( binding.binding, loaded.loaded,
					  wtf.wtf ) ) {
			*veto = &efi_vetoes[i];
			break;
		}
	}

	/* Success */
	rc = 0;

	/* Close protocols */
	if ( wtf.wtf ) {
		bs->CloseProtocol ( driver, &efi_component_name_protocol_guid,
				    efi_image_handle, driver );
	}
	bs->CloseProtocol ( image, &efi_loaded_image_protocol_guid,
			    efi_image_handle, image );
 err_loaded:
	bs->CloseProtocol ( driver, &efi_driver_binding_protocol_guid,
			    efi_image_handle, driver );
 err_binding:
	return rc;
}

/**
 * Unload any vetoed drivers
 *
 */
void efi_veto_unload ( void ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct efi_veto *veto;
	EFI_HANDLE *drivers;
	EFI_HANDLE driver;
	UINTN num_drivers;
	unsigned int i;
	EFI_STATUS efirc;
	int rc;

	/* Locate all driver binding protocol handles */
	if ( ( efirc = bs->LocateHandleBuffer (
			ByProtocol, &efi_driver_binding_protocol_guid,
			NULL, &num_drivers, &drivers ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( &efi_vetoes, "EFIVETO could not list all drivers: "
		       "%s\n", strerror ( rc ) );
		return;
	}

	/* Unload any vetoed drivers */
	for ( i = 0 ; i < num_drivers ; i++ ) {
		driver = drivers[i];
		if ( ( rc = efi_veto ( driver, &veto ) ) != 0 ) {
			DBGC ( driver, "EFIVETO could not determine "
			       "vetoing for %s: %s\n",
			       efi_handle_name ( driver ), strerror ( rc ) );
			continue;
		}
		if ( ! veto )
			continue;
		DBGC ( driver, "EFIVETO unloading %s (%s)\n",
		       efi_handle_name ( driver ), veto->name );
		if ( ( efirc = bs->UnloadImage ( driver ) ) != 0 ) {
			rc = -EEFI ( efirc );
			DBGC ( driver, "EFIVETO could not unload %s: %s\n",
			       efi_handle_name ( driver ), strerror ( rc ) );
		}
	}

	/* Free handle list */
	bs->FreePool ( drivers );
}
