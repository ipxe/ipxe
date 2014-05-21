/*
 * Copyright (C) 2011 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/DriverBinding.h>
#include <ipxe/efi/Protocol/ComponentName2.h>
#include <ipxe/efi/efi_strings.h>
#include <ipxe/efi/efi_driver.h>
#include <config/general.h>

/** @file
 *
 * EFI driver interface
 *
 */

/** EFI driver binding protocol GUID */
static EFI_GUID efi_driver_binding_protocol_guid
	= EFI_DRIVER_BINDING_PROTOCOL_GUID;

/** EFI component name protocol GUID */
static EFI_GUID efi_component_name2_protocol_guid
	= EFI_COMPONENT_NAME2_PROTOCOL_GUID;

/**
 * Find end of device path
 *
 * @v path		Path to device
 * @ret path_end	End of device path
 */
EFI_DEVICE_PATH_PROTOCOL * efi_devpath_end ( EFI_DEVICE_PATH_PROTOCOL *path ) {

	while ( path->Type != END_DEVICE_PATH_TYPE ) {
		path = ( ( ( void * ) path ) +
			 /* There's this amazing new-fangled thing known as
			  * a UINT16, but who wants to use one of those? */
			 ( ( path->Length[1] << 8 ) | path->Length[0] ) );
	}

	return path;
}

/**
 * Look up driver name
 *
 * @v wtf		Component name protocol
 * @v language		Language to use
 * @v driver_name	Driver name to fill in
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_driver_get_driver_name ( EFI_COMPONENT_NAME2_PROTOCOL *wtf,
			     CHAR8 *language __unused, CHAR16 **driver_name ) {
	struct efi_driver *efidrv =
		container_of ( wtf, struct efi_driver, wtf );

	*driver_name = efidrv->wname;
	return 0;
}

/**
 * Look up controller name
 *
 * @v wtf		Component name protocol
 * @v device		Device
 * @v child		Child device, or NULL
 * @v language		Language to use
 * @v driver_name	Device name to fill in
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_driver_get_controller_name ( EFI_COMPONENT_NAME2_PROTOCOL *wtf __unused,
				 EFI_HANDLE device, EFI_HANDLE child,
				 CHAR8 *language, CHAR16 **controller_name ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	union {
		EFI_COMPONENT_NAME2_PROTOCOL *name2;
		void *interface;
	} name2;
	EFI_STATUS efirc;

	/* Delegate to the EFI_COMPONENT_NAME2_PROTOCOL instance
	 * installed on child handle, if present.
	 */
	if ( ( child != NULL ) &&
	     ( ( efirc = bs->OpenProtocol (
			  child, &efi_component_name2_protocol_guid,
			  &name2.interface, NULL, NULL,
			  EFI_OPEN_PROTOCOL_GET_PROTOCOL ) ) == 0 ) ) {
		return name2.name2->GetControllerName ( name2.name2, device,
							child, language,
							controller_name );
	}

	/* Otherwise, let EFI use the default Device Path Name */
	return EFI_UNSUPPORTED;
}

/**
 * Try to connect EFI driver
 *
 * @v efidrv		EFI driver
 * @v handle		Controller handle
 */
static void efi_driver_connect ( struct efi_driver *efidrv, EFI_HANDLE handle ){
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE drivers[2] = { efidrv->driver.DriverBindingHandle, NULL };

	bs->ConnectController ( handle, drivers, NULL, FALSE );
}

/**
 * Try to disconnect EFI driver
 *
 * @v efidrv		EFI driver
 * @v handle		Controller handle
 */
static void efi_driver_disconnect ( struct efi_driver *efidrv,
				    EFI_HANDLE handle ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;

	bs->DisconnectController ( handle, efidrv->driver.DriverBindingHandle,
				   NULL );
}

/**
 * Connect/disconnect EFI driver from all handles
 *
 * @v efidrv		EFI driver
 * @v method		Connect/disconnect method
 * @ret rc		Return status code
 */
static int efi_driver_handles ( struct efi_driver *efidrv,
				void ( * method ) ( struct efi_driver *efidrv,
						    EFI_HANDLE handle ) ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE *handles;
	UINTN num_handles;
	EFI_STATUS efirc;
	UINTN i;
	int rc;

	/* Enumerate all handles */
	if ( ( efirc = bs->LocateHandleBuffer ( AllHandles, NULL, NULL,
						&num_handles,
						&handles ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( efidrv, "EFIDRV %s could not list handles: %s\n",
		       efidrv->name, strerror ( rc ) );
		return rc;
	}

	/* Connect/disconnect driver from all handles */
	for ( i = 0 ; i < num_handles ; i++ )
		method ( efidrv, handles[i] );

	/* Free list of handles */
	bs->FreePool ( handles );

	return 0;
}

/**
 * Install EFI driver
 *
 * @v efidrv		EFI driver
 * @ret rc		Return status code
 */
int efi_driver_install ( struct efi_driver *efidrv ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_DRIVER_BINDING_PROTOCOL *driver = &efidrv->driver;
	EFI_COMPONENT_NAME2_PROTOCOL *wtf = &efidrv->wtf;
	EFI_STATUS efirc;
	int rc;

	/* Configure driver binding protocol */
	driver->ImageHandle = efi_image_handle;

	/* Configure component name protocol */
	wtf->GetDriverName = efi_driver_get_driver_name;
	wtf->GetControllerName = efi_driver_get_controller_name;
	wtf->SupportedLanguages = "en";

	/* Fill in driver name */
	efi_snprintf ( efidrv->wname,
		       ( sizeof ( efidrv->wname ) /
			 sizeof ( efidrv->wname[0] ) ),
		       PRODUCT_SHORT_NAME "%s%s",
		       ( efidrv->name ? " - " : "" ),
		       ( efidrv->name ? efidrv->name : "" ) );

	/* Install driver */
	if ( ( efirc = bs->InstallMultipleProtocolInterfaces (
			&driver->DriverBindingHandle,
			&efi_driver_binding_protocol_guid, driver,
			&efi_component_name2_protocol_guid, wtf,
			NULL ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( efidrv, "EFIDRV %s could not install protocol: %s\n",
		       efidrv->name, strerror ( rc ) );
		return rc;
	}

	/* Connect devices */
	DBGC ( efidrv, "EFIDRV %s connecting devices\n", efidrv->name );
	efi_driver_handles ( efidrv, efi_driver_connect );

	DBGC ( efidrv, "EFIDRV %s installed\n", efidrv->name );
	return 0;
}

/**
 * Uninstall EFI driver
 *
 * @v efidrv		EFI driver
 */
void efi_driver_uninstall ( struct efi_driver *efidrv ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;

	/* Disconnect the driver from its devices */
	DBGC ( efidrv, "EFIDRV %s disconnecting devices\n", efidrv->name );
	efi_driver_handles ( efidrv, efi_driver_disconnect );

	/* Uninstall the driver */
	bs->UninstallMultipleProtocolInterfaces (
			efidrv->driver.DriverBindingHandle,
			&efi_driver_binding_protocol_guid, &efidrv->driver,
			&efi_component_name2_protocol_guid, &efidrv->wtf,
			NULL );
	DBGC ( efidrv, "EFIDRV %s uninstalled\n", efidrv->name );
}
