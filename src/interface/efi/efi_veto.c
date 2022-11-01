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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ipxe/settings.h>
#include <ipxe/pci.h>
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
	 * @v manufacturer	Manufacturer name, if present
	 * @v name		Driver name (in "eng" language), if present
	 * @ret vetoed		Driver is to be vetoed
	 */
	int ( * veto ) ( EFI_DRIVER_BINDING_PROTOCOL *binding,
			 EFI_LOADED_IMAGE_PROTOCOL *loaded,
			 EFI_COMPONENT_NAME_PROTOCOL *wtf,
			 const char *manufacturer, const CHAR16 *name );
};

/**
 * Unload an EFI driver
 *
 * @v driver		Driver binding handle
 * @ret rc		Return status code
 */
static int efi_veto_unload ( EFI_HANDLE driver ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_STATUS efirc;
	int rc;

	/* Unload the driver */
	if ( ( efirc = bs->UnloadImage ( driver ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( driver, "EFIVETO %s could not unload: %s\n",
		       efi_handle_name ( driver ), strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Disconnect an EFI driver from all handles
 *
 * @v driver		Driver binding handle
 * @ret rc		Return status code
 */
static int efi_veto_disconnect ( EFI_HANDLE driver ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE *handles;
	EFI_HANDLE handle;
	UINTN count;
	unsigned int i;
	EFI_STATUS efirc;
	int rc;

	/* Enumerate all handles */
	if ( ( efirc = bs->LocateHandleBuffer ( AllHandles, NULL, NULL,
						&count, &handles ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( driver, "EFIVETO %s could not enumerate handles: %s\n",
		       efi_handle_name ( driver ), strerror ( rc ) );
		goto err_list;
	}

	/* Disconnect driver from all handles, in reverse order */
	for ( i = 0 ; i < count ; i++ ) {
		handle = handles[ count - i - 1 ];
		efirc = bs->DisconnectController ( handle, driver, NULL );
		if ( ( efirc != 0 ) && ( efirc != EFI_NOT_FOUND ) ) {
			rc = -EEFI ( efirc );
			DBGC ( driver, "EFIVETO %s could not disconnect",
			       efi_handle_name ( driver ) );
			DBGC ( driver, " %s: %s\n",
			       efi_handle_name ( handle ), strerror ( rc ) );
			goto err_disconnect;
		}
	}

	/* Success */
	rc = 0;
	DBGC2 ( driver, "EFIVETO %s disconnected all handles\n",
		efi_handle_name ( driver ) );

 err_disconnect:
	bs->FreePool ( handles );
 err_list:
	return rc;
}

/**
 * Uninstall an EFI driver binding protocol
 *
 * @v driver		Driver binding handle
 * @ret rc		Return status code
 */
static int efi_veto_uninstall ( EFI_HANDLE driver ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	union {
		EFI_DRIVER_BINDING_PROTOCOL *binding;
		void *interface;
	} binding;
	EFI_STATUS efirc;
	int rc;

	/* Open driver binding protocol */
	if ( ( efirc = bs->OpenProtocol (
			driver, &efi_driver_binding_protocol_guid,
			&binding.interface, efi_image_handle, driver,
			EFI_OPEN_PROTOCOL_GET_PROTOCOL ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( driver, "EFIVETO %s could not open driver binding "
		       "protocol: %s\n", efi_handle_name ( driver ),
		       strerror ( rc ) );
		return rc;
	}

	/* Close driver binding protocol */
	bs->CloseProtocol ( driver, &efi_driver_binding_protocol_guid,
			    efi_image_handle, driver );

	/* Uninstall driver binding protocol */
	if ( ( efirc = bs->UninstallMultipleProtocolInterfaces (
			driver, &efi_driver_binding_protocol_guid,
			binding.binding, NULL ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( driver, "EFIVETO %s could not uninstall driver "
		       "binding protocol: %s\n",
		       efi_handle_name ( driver ), strerror ( rc ) );
		return rc;
	}

	DBGC2 ( driver, "EFIVETO %s uninstalled driver binding protocol\n",
		efi_handle_name ( driver ) );
	return 0;
}

/**
 * Close protocol on handle potentially opened by an EFI driver
 *
 * @v driver		Driver binding handle
 * @v handle		Potentially opened handle
 * @v protocol		Opened protocol
 * @ret rc		Return status code
 */
static int efi_veto_close_protocol ( EFI_HANDLE driver, EFI_HANDLE handle,
				     EFI_GUID *protocol ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *openers;
	EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *opener;
	EFI_HANDLE controller;
	UINTN count;
	unsigned int i;
	EFI_STATUS efirc;
	int rc;

	/* Retrieve list of openers */
	if ( ( efirc = bs->OpenProtocolInformation ( handle, protocol, &openers,
						     &count ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( driver, "EFIVETO %s could not retrieve openers",
		       efi_handle_name ( driver ) );
		DBGC ( driver, " of %s %s: %s", efi_handle_name ( handle ),
		       efi_guid_ntoa ( protocol ), strerror ( rc ) );
		goto err_list;
	}

	/* Close anything opened by this driver */
	for ( i = 0 ; i < count ; i++ ) {
		opener = &openers[i];
		if ( opener->AgentHandle != driver )
			continue;
		controller = opener->ControllerHandle;
		DBGC_EFI_OPENER ( driver, handle, protocol, opener );
		if ( ( efirc = bs->CloseProtocol ( handle, protocol, driver,
						   controller ) ) != 0 ) {
			rc = -EEFI ( efirc );
			DBGC ( driver, "EFIVETO %s could not close stray open",
			       efi_handle_name ( driver ) );
			DBGC ( driver, " of %s: %s\n",
			       efi_handle_name ( handle ), strerror ( rc ) );
			goto err_close;
		}
	}

	/* Success */
	rc = 0;

 err_close:
	bs->FreePool ( openers );
 err_list:
	return rc;
}

/**
 * Close handle potentially opened by an EFI driver
 *
 * @v driver		Driver binding handle
 * @v handle		Potentially opened handle
 * @ret rc		Return status code
 */
static int efi_veto_close_handle ( EFI_HANDLE driver, EFI_HANDLE handle ) {
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
		DBGC ( driver, "EFIVETO %s could not retrieve protocols",
		       efi_handle_name ( driver ) );
		DBGC ( driver, " for %s: %s\n",
		       efi_handle_name ( handle ), strerror ( rc ) );
		goto err_list;
	}

	/* Close each protocol */
	for ( i = 0 ; i < count ; i++ ) {
		if ( ( rc = efi_veto_close_protocol ( driver, handle,
						      protocols[i] ) ) != 0 )
			goto err_close;
	}

	/* Success */
	rc = 0;

 err_close:
	bs->FreePool ( protocols );
 err_list:
	return rc;
}

/**
 * Close all remaining handles opened by an EFI driver
 *
 * @v driver		Driver binding handle
 * @ret rc		Return status code
 */
static int efi_veto_close ( EFI_HANDLE driver ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE *handles;
	UINTN count;
	unsigned int i;
	EFI_STATUS efirc;
	int rc;

	/* Enumerate all handles */
	if ( ( efirc = bs->LocateHandleBuffer ( AllHandles, NULL, NULL,
						&count, &handles ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( driver, "EFIVETO %s could not enumerate handles: %s\n",
		       efi_handle_name ( driver ), strerror ( rc ) );
		goto err_list;
	}

	/* Close each handle */
	for ( i = 0 ; i < count ; i++ ) {
		if ( ( rc = efi_veto_close_handle ( driver,
						    handles[i] ) ) != 0 )
			goto err_close;
	}

	/* Success */
	rc = 0;
	DBGC2 ( driver, "EFIVETO %s closed all remaining handles\n",
		efi_handle_name ( driver ) );

 err_close:
	bs->FreePool ( handles );
 err_list:
	return rc;
}

/**
 * Terminate an EFI driver with extreme prejudice
 *
 * @v driver		Driver binding handle
 * @ret rc		Return status code
 */
static int efi_veto_destroy ( EFI_HANDLE driver ) {
	int rc;

	/* Disconnect driver from all handles */
	if ( ( rc = efi_veto_disconnect ( driver ) ) != 0 )
		return rc;

	/* Uninstall driver binding protocol */
	if ( ( rc = efi_veto_uninstall ( driver ) ) != 0 )
		return rc;

	/* Close any remaining opened handles */
	if ( ( rc = efi_veto_close ( driver ) ) != 0 )
		return rc;

	DBGC ( driver, "EFIVETO %s forcibly removed\n",
	       efi_handle_name ( driver ) );
	return 0;
}

/**
 * Veto an EFI driver
 *
 * @v driver		Driver binding handle
 * @ret rc		Return status code
 */
static int efi_veto_driver ( EFI_HANDLE driver ) {
	int rc;

	/* Try gracefully unloading the driver */
	if ( ( rc = efi_veto_unload ( driver ) ) == 0 )
		return 0;

	/* If that fails, use a hammer */
	if ( ( rc = efi_veto_destroy ( driver ) ) == 0 )
		return 0;

	return rc;
}

/**
 * Veto Dell Ip4ConfigDxe driver
 *
 * @v binding		Driver binding protocol
 * @v loaded		Loaded image protocol
 * @v wtf		Component name protocol, if present
 * @v manufacturer	Manufacturer name, if present
 * @v name		Driver name, if present
 * @ret vetoed		Driver is to be vetoed
 */
static int
efi_veto_dell_ip4config ( EFI_DRIVER_BINDING_PROTOCOL *binding __unused,
			  EFI_LOADED_IMAGE_PROTOCOL *loaded __unused,
			  EFI_COMPONENT_NAME_PROTOCOL *wtf __unused,
			  const char *manufacturer, const CHAR16 *name ) {
	static const CHAR16 ip4cfg[] = L"IP4 CONFIG Network Service Driver";
	static const char *dell = "Dell Inc.";

	/* Check manufacturer and driver name */
	if ( ! manufacturer )
		return 0;
	if ( ! name )
		return 0;
	if ( strcmp ( manufacturer, dell ) != 0 )
		return 0;
	if ( memcmp ( name, ip4cfg, sizeof ( ip4cfg ) ) != 0 )
		return 0;

	return 1;
}

/**
 * Veto HP XhciDxe driver
 *
 * @v binding		Driver binding protocol
 * @v loaded		Loaded image protocol
 * @v wtf		Component name protocol, if present
 * @v manufacturer	Manufacturer name, if present
 * @v name		Driver name, if present
 * @ret vetoed		Driver is to be vetoed
 */
static int
efi_veto_hp_xhci ( EFI_DRIVER_BINDING_PROTOCOL *binding __unused,
		   EFI_LOADED_IMAGE_PROTOCOL *loaded __unused,
		   EFI_COMPONENT_NAME_PROTOCOL *wtf __unused,
		   const char *manufacturer, const CHAR16 *name ) {
	static const CHAR16 xhci[] = L"Usb Xhci Driver";
	static const char *hp = "HP";
	struct pci_driver *driver;

	/* Check manufacturer and driver name */
	if ( ! manufacturer )
		return 0;
	if ( ! name )
		return 0;
	if ( strcmp ( manufacturer, hp ) != 0 )
		return 0;
	if ( memcmp ( name, xhci, sizeof ( xhci ) ) != 0 )
		return 0;

	/* Veto driver only if we have our own xHCI driver */
	for_each_table_entry ( driver, PCI_DRIVERS ) {
		if ( driver->class.class ==
		     PCI_CLASS ( PCI_CLASS_SERIAL, PCI_CLASS_SERIAL_USB,
				 PCI_CLASS_SERIAL_USB_XHCI ) ) {
			return 1;
		}
	}

	return 0;
}

/** Driver vetoes */
static struct efi_veto efi_vetoes[] = {
	{
		.name = "Dell Ip4Config",
		.veto = efi_veto_dell_ip4config,
	},
	{
		.name = "HP Xhci",
		.veto = efi_veto_hp_xhci,
	},
};

/**
 * Find driver veto, if any
 *
 * @v driver		Driver binding handle
 * @v manufacturer	Manufacturer name, if present
 * @ret veto		Driver veto, or NULL
 * @ret rc		Return status code
 */
static int efi_veto_find ( EFI_HANDLE driver, const char *manufacturer,
			   struct efi_veto **veto ) {
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
	CHAR16 *name;
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

	/* Get driver name, if available */
	if ( wtf.wtf &&
	     ( ( efirc = wtf.wtf->GetDriverName ( wtf.wtf, "eng",
						  &name ) == 0 ) ) ) {
		/* Driver has a name */
	} else {
		/* Ignore failure; name is not required to be present */
		name = NULL;
	}

	/* Check vetoes */
	for ( i = 0 ; i < ( sizeof ( efi_vetoes ) /
			    sizeof ( efi_vetoes[0] ) ) ; i++ ) {
		if ( efi_vetoes[i].veto ( binding.binding, loaded.loaded,
					  wtf.wtf, manufacturer, name ) ) {
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
 * Remove any vetoed drivers
 *
 */
void efi_veto ( void ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct efi_veto *veto;
	EFI_HANDLE *drivers;
	EFI_HANDLE driver;
	UINTN num_drivers;
	unsigned int i;
	char *manufacturer;
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

	/* Get manufacturer name */
	fetch_string_setting_copy ( NULL, &manufacturer_setting,
				    &manufacturer );

	/* Unload any vetoed drivers */
	for ( i = 0 ; i < num_drivers ; i++ ) {
		driver = drivers[i];
		if ( ( rc = efi_veto_find ( driver, manufacturer,
					    &veto ) ) != 0 ) {
			DBGC ( driver, "EFIVETO %s could not determine "
			       "vetoing: %s\n",
			       efi_handle_name ( driver ), strerror ( rc ) );
			continue;
		}
		if ( ! veto )
			continue;
		DBGC ( driver, "EFIVETO %s is vetoed (%s)\n",
		       efi_handle_name ( driver ), veto->name );
		if ( ( rc = efi_veto_driver ( driver ) ) != 0 ) {
			DBGC ( driver, "EFIVETO %s could not veto: %s\n",
			       efi_handle_name ( driver ), strerror ( rc ) );
		}
	}

	/* Free manufacturer name */
	free ( manufacturer );

	/* Free handle list */
	bs->FreePool ( drivers );
}
