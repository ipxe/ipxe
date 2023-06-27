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
	 * @v manufacturer	Manufacturer name, if present
	 * @v name		Driver name (in "eng" language), if present
	 * @ret vetoed		Driver is to be vetoed
	 */
	int ( * veto ) ( const char *manufacturer, const CHAR16 *name );
};

/**
 * Unload an EFI driver
 *
 * @v driver		Driver binding handle
 * @v image		Image handle to fill in on failure
 * @ret rc		Return status code
 */
static int efi_veto_unload ( EFI_HANDLE driver, EFI_HANDLE *image ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	union {
		EFI_DRIVER_BINDING_PROTOCOL *binding;
		void *interface;
	} binding;
	EFI_STATUS efirc;
	int rc;

	/* Clear image handle */
	*image = NULL;

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

	/* Identify image handle */
	*image = binding.binding->ImageHandle;

	/* Unload the driver */
	if ( ( efirc = bs->UnloadImage ( *image ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( driver, "EFIVETO %s could not unload",
		       efi_handle_name ( driver ) );
		DBGC ( driver, " %s: %s\n",
		       efi_handle_name ( *image ), strerror ( rc ) );
		goto err_unload;
	}

	/* Invalidate now-unloaded image handle */
	*image = NULL;

 err_unload:
	bs->CloseProtocol ( driver, &efi_driver_binding_protocol_guid,
			    efi_image_handle, driver );
 err_binding:
	return rc;
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
 * Uninstall an EFI protocol
 *
 * @v agent		Agent handle
 * @v loaded		Loaded image protocol filter, or NULL
 * @v handle		Potentially opened handle
 * @v protocol		Installed protocol
 * @ret rc		Return status code
 */
static int efi_veto_uninstall ( EFI_HANDLE agent,
				EFI_LOADED_IMAGE_PROTOCOL *loaded,
				EFI_HANDLE handle, EFI_GUID *protocol ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *interface;
	void *method;
	size_t offset;
	EFI_STATUS efirc;
	int rc;

	/* Open protocol */
	if ( ( efirc = bs->OpenProtocol (
			handle, protocol, &interface, efi_image_handle, agent,
			EFI_OPEN_PROTOCOL_GET_PROTOCOL ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( agent, "EFIVETO %s could not open %s",
		       efi_handle_name ( agent ), efi_guid_ntoa ( protocol ) );
		DBGC ( agent, " on %s: %s\n",
		       efi_handle_name ( handle ), strerror ( rc ) );
		return rc;
	}

	/* Get representative method pointer, if known */
	method = efi_veto_method ( protocol, interface );

	/* Close protocol */
	bs->CloseProtocol ( handle, protocol, efi_image_handle, agent );

	/* Do nothing if method does not match filter */
	if ( loaded ) {
		offset = ( method - loaded->ImageBase );
		if ( offset > loaded->ImageSize )
			return 0;
	}

	/* Print debug message before potential invalidation of handle */
	DBGC2 ( agent, "EFIVETO %s uninstalling %s",
		efi_handle_name ( agent ), efi_guid_ntoa ( protocol ) );
	DBGC2 ( agent, " from %s\n", efi_handle_name ( handle ) );

	/* Uninstall protocol */
	if ( ( efirc = bs->UninstallMultipleProtocolInterfaces (
			handle, protocol, interface, NULL ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( agent, "EFIVETO %s could not uninstall %s",
		       efi_handle_name ( agent ), efi_guid_ntoa ( protocol ) );
		DBGC ( agent, " from %s: %s\n",
		       efi_handle_name ( handle ), strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Close protocol on handle potentially opened by an EFI driver
 *
 * @v agent		Agent handle
 * @v handle		Potentially opened handle
 * @v protocol		Opened protocol
 * @ret rc		Return status code
 */
static int efi_veto_close ( EFI_HANDLE agent, EFI_HANDLE handle,
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
		DBGC ( agent, "EFIVETO %s could not retrieve openers",
		       efi_handle_name ( agent ) );
		DBGC ( agent, " of %s %s: %s", efi_handle_name ( handle ),
		       efi_guid_ntoa ( protocol ), strerror ( rc ) );
		goto err_list;
	}

	/* Close anything opened by this driver */
	for ( i = 0 ; i < count ; i++ ) {
		opener = &openers[ count - i - 1 ];
		if ( opener->AgentHandle != agent )
			continue;
		controller = opener->ControllerHandle;
		DBGC_EFI_OPENER ( agent, handle, protocol, opener );
		if ( ( efirc = bs->CloseProtocol ( handle, protocol, agent,
						   controller ) ) != 0 ) {
			rc = -EEFI ( efirc );
			DBGC ( agent, "EFIVETO %s could not close stray open",
			       efi_handle_name ( agent ) );
			DBGC ( agent, " of %s %s: %s\n",
			       efi_handle_name ( handle ),
			       efi_guid_ntoa ( protocol ), strerror ( rc ) );
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
 * Close/uninstall handle potentially used by an EFI driver
 *
 * @v agent		Agent handle
 * @v loaded		Loaded image protocol for uninstallation, or NULL
 * @v handle		Potentially opened handle
 * @ret rc		Return status code
 */
static int efi_veto_handle ( EFI_HANDLE agent,
			     EFI_LOADED_IMAGE_PROTOCOL *loaded,
			     EFI_HANDLE handle ) {
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
		DBGC ( agent, "EFIVETO %s could not retrieve protocols",
		       efi_handle_name ( agent ) );
		DBGC ( agent, " for %s: %s\n",
		       efi_handle_name ( handle ), strerror ( rc ) );
		goto err_list;
	}

	/* Close each protocol */
	for ( i = 0 ; i < count ; i++ ) {
		protocol = protocols[ count - i - 1];
		if ( ( rc = efi_veto_close ( agent, handle, protocol ) ) != 0 )
			goto err_close;
	}

	/* Uninstall each protocol, if applicable */
	if ( loaded ) {
		for ( i = 0 ; i < count ; i++ ) {
			protocol = protocols[ count - i - 1];
			if ( ( rc = efi_veto_uninstall ( agent, loaded, handle,
							 protocol ) ) != 0 )
				goto err_uninstall;
		}
	}

	/* Success */
	rc = 0;

 err_uninstall:
 err_close:
	bs->FreePool ( protocols );
 err_list:
	return rc;
}

/**
 * Close/uninstall all remaining handles used by an EFI driver
 *
 * @v agent		Agent handle
 * @v loaded		Loaded image protocol for uninstallation, or NULL
 * @ret rc		Return status code
 */
static int efi_veto_handles ( EFI_HANDLE agent,
			      EFI_LOADED_IMAGE_PROTOCOL *loaded ) {
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
		DBGC ( agent, "EFIVETO %s could not enumerate handles: %s\n",
		       efi_handle_name ( agent ), strerror ( rc ) );
		goto err_list;
	}

	/* Close and/or uninstall each handle */
	for ( i = 0 ; i < count ; i++ ) {
		handle = handles[ count - i - 1 ];
		if ( ( rc = efi_veto_handle ( agent, loaded, handle ) ) != 0 )
			goto err_handle;
	}

	/* Success */
	rc = 0;
	DBGC2 ( agent, "EFIVETO %s closed%s all remaining handles\n",
		efi_handle_name ( agent ),
		( loaded ? " and uninstalled" : "" ) );

 err_handle:
	bs->FreePool ( handles );
 err_list:
	return rc;
}

/**
 * Terminate an EFI driver with extreme prejudice
 *
 * @v driver		Driver handle
 * @ret rc		Return status code
 */
static int efi_veto_destroy ( EFI_HANDLE driver ) {
	EFI_GUID *binding = &efi_driver_binding_protocol_guid;
	int rc;

	/* Disconnect driver from all handles */
	if ( ( rc = efi_veto_disconnect ( driver ) ) != 0 )
		return rc;

	/* Uninstall driver binding protocol */
	if ( ( rc = efi_veto_uninstall ( driver, NULL, driver,
					 binding ) ) != 0 )
		return rc;

	/* Close any remaining opened handles */
	if ( ( rc = efi_veto_handles ( driver, NULL ) ) != 0 )
		return rc;

	DBGC ( driver, "EFIVETO %s forcibly removed driver\n",
	       efi_handle_name ( driver ) );
	return 0;
}

/**
 * Veto an EFI driver handle
 *
 * @v driver		Driver handle
 * @ret image		Image handle, if still present after veto attempt
 * @ret rc		Return status code
 */
static int efi_veto_driver ( EFI_HANDLE driver, EFI_HANDLE *image ) {
	int rc;

	/* Try gracefully unloading the underlying image */
	if ( ( rc = efi_veto_unload ( driver, image ) ) == 0 )
		return 0;

	/* If that fails, use a hammer */
	if ( ( rc = efi_veto_destroy ( driver ) ) == 0 )
		return 0;

	DBGC ( driver, "EFIVETO %s could not veto driver: %s\n",
	       efi_handle_name ( driver ), strerror ( rc ) );
	return rc;
}

/**
 * Veto an EFI image handle
 *
 * @v image		Image handle
 * @ret rc		Return status code
 */
static int efi_veto_image ( EFI_HANDLE image ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	union {
		EFI_LOADED_IMAGE_PROTOCOL *loaded;
		void *interface;
	} loaded;
	EFI_STATUS efirc;
	int rc;

	/* Open loaded image protocol */
	if ( ( efirc = bs->OpenProtocol (
			image, &efi_loaded_image_protocol_guid,
			&loaded.interface, efi_image_handle, image,
			EFI_OPEN_PROTOCOL_GET_PROTOCOL ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( image, "EFIVETO %s could not open loaded image "
		       "protocol: %s\n", efi_handle_name ( image ),
		       strerror ( rc ) );
		goto err_loaded;
	}

	/* Close and uninstall any remaining handles */
	if ( ( rc = efi_veto_handles ( image, loaded.loaded ) ) != 0 )
		return rc;

	DBGC ( image, "EFIVETO %s forcibly removed image\n",
	       efi_handle_name ( image ) );

	bs->CloseProtocol ( image, &efi_loaded_image_protocol_guid,
			    efi_image_handle, image );
 err_loaded:
	return rc;
}

/**
 * Veto Ip4ConfigDxe driver on some platforms
 *
 * @v manufacturer	Manufacturer name, if present
 * @v name		Driver name, if present
 * @ret vetoed		Driver is to be vetoed
 */
static int efi_veto_ip4config ( const char *manufacturer,
				const CHAR16 *name ) {
	static const CHAR16 ip4cfg[] = L"IP4 CONFIG Network Service Driver";
	static const char *dell = "Dell Inc.";
	static const char *itautec = "Itautec S.A.";

	/* Check manufacturer and driver name */
	if ( ! manufacturer )
		return 0;
	if ( ! name )
		return 0;
	if ( ( strcmp ( manufacturer, dell ) != 0 ) &&
	     ( strcmp ( manufacturer, itautec ) != 0 ) )
		return 0;
	if ( memcmp ( name, ip4cfg, sizeof ( ip4cfg ) ) != 0 )
		return 0;

	return 1;
}

/**
 * Veto HP XhciDxe driver
 *
 * @v manufacturer	Manufacturer name, if present
 * @v name		Driver name, if present
 * @ret vetoed		Driver is to be vetoed
 */
static int efi_veto_hp_xhci ( const char *manufacturer, const CHAR16 *name ) {
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

/**
 * Veto VMware UefiPxeBcDxe driver
 *
 * @v binding		Driver binding protocol
 * @v wtf		Component name protocol, if present
 * @v manufacturer	Manufacturer name, if present
 * @v name		Driver name, if present
 * @ret vetoed		Driver is to be vetoed
 */
static int efi_veto_vmware_uefipxebc ( const char *manufacturer,
				       const CHAR16 *name ) {
	static const CHAR16 uefipxebc[] = L"UEFI PXE Base Code Driver";
	static const char *vmware = "VMware, Inc.";

	/* Check manufacturer and driver name */
	if ( ! manufacturer )
		return 0;
	if ( ! name )
		return 0;
	if ( strcmp ( manufacturer, vmware ) != 0 )
		return 0;
	if ( memcmp ( name, uefipxebc, sizeof ( uefipxebc ) ) != 0 )
		return 0;

	return 1;
}

/**
 * Veto VirtualBox E1kNetDxe driver
 *
 * @v manufacturer	Manufacturer name, if present
 * @v name		Driver name, if present
 * @ret vetoed		Driver is to be vetoed
 */
static int efi_veto_vbox_e1knet ( const char *manufacturer,
				  const CHAR16 *name ) {
	static const CHAR16 e1knet[] = L"E1000 network interface card Driver";
	static const char *vbox = "innotek GmbH";

	/* Check manufacturer and driver name */
	if ( ! manufacturer )
		return 0;
	if ( ! name )
		return 0;
	if ( strcmp ( manufacturer, vbox ) != 0 )
		return 0;
	if ( memcmp ( name, e1knet, sizeof ( e1knet ) ) != 0 )
		return 0;

	return 1;
}

/** Driver vetoes */
static struct efi_veto efi_vetoes[] = {
	{
		.name = "Ip4Config",
		.veto = efi_veto_ip4config,
	},
	{
		.name = "HP Xhci",
		.veto = efi_veto_hp_xhci,
	},
	{
		.name = "VMware UefiPxeBc",
		.veto = efi_veto_vmware_uefipxebc,
	},
	{
		.name = "VirtualBox E1kNetDxe",
		.veto = efi_veto_vbox_e1knet,
	},
};

/**
 * Find driver veto, if any
 *
 * @v driver		Driver binding handle
 * @v manufacturer	Manufacturer name, if present
 * @ret veto		Driver veto to fill in
 * @ret rc		Return status code
 */
static int efi_veto_find ( EFI_HANDLE driver, const char *manufacturer,
			   struct efi_veto **veto ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	union {
		EFI_COMPONENT_NAME_PROTOCOL *wtf;
		void *interface;
	} wtf;
	CHAR16 *name;
	unsigned int i;
	EFI_STATUS efirc;

	DBGC2 ( &efi_vetoes, "EFIVETO checking %s\n",
		efi_handle_name ( driver ) );

	/* Mark as not vetoed */
	*veto = NULL;

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

		/* Check if this veto applies */
		if ( ! efi_vetoes[i].veto ( manufacturer, name ) )
			continue;

		DBGC ( driver, "EFIVETO %s is vetoed (%s)\n",
		       efi_handle_name ( driver ), efi_vetoes[i].name );
		*veto = &efi_vetoes[i];

		break;
	}

	/* Close protocols */
	if ( wtf.wtf ) {
		bs->CloseProtocol ( driver, &efi_component_name_protocol_guid,
				    efi_image_handle, driver );
	}

	return 0;
}

/**
 * Remove any vetoed drivers
 *
 */
void efi_veto ( void ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct efi_veto *veto;
	EFI_HANDLE *targets;
	EFI_HANDLE driver;
	EFI_HANDLE image;
	UINTN count;
	unsigned int i;
	char *manufacturer;
	EFI_STATUS efirc;
	int rc;

	/* Locate all driver binding protocol handles */
	if ( ( efirc = bs->LocateHandleBuffer (
			ByProtocol, &efi_driver_binding_protocol_guid,
			NULL, &count, &targets ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( &efi_vetoes, "EFIVETO could not list all drivers: "
		       "%s\n", strerror ( rc ) );
		return;
	}

	/* Get manufacturer name */
	fetch_string_setting_copy ( NULL, &manufacturer_setting,
				    &manufacturer );
	DBGC ( &efi_vetoes, "EFIVETO manufacturer is \"%s\"\n", manufacturer );

	/* Identify any vetoed drivers and veto the driver handles */
	for ( i = 0 ; i < count ; i++ ) {

		/* Consume driver handle from target list */
		driver = targets[ count - i - 1 ];
		targets[ count - i - 1 ] = NULL;

		/* Check if this driver is vetoed */
		if ( ( rc = efi_veto_find ( driver, manufacturer,
					    &veto ) ) != 0 ) {
			DBGC ( driver, "EFIVETO %s could not determine "
			       "vetoing: %s\n",
			       efi_handle_name ( driver ), strerror ( rc ) );
			continue;
		}
		if ( ! veto )
			continue;

		/* Veto driver handle */
		if ( ( rc = efi_veto_driver ( driver, &image ) ) != 0 )
			continue;

		/* Record image handle (if still present) as a target */
		targets[ count - i - 1 ] = image;
	}

	/* Veto any remaining image handles */
	for ( i = 0 ; i < count ; i++ ) {

		/* Consume image handle from target list */
		image = targets[ count - i - 1 ];
		if ( ! image )
			continue;

		/* Veto image handle */
		if ( ( rc = efi_veto_image ( image ) ) != 0 )
			continue;
	}

	/* Free manufacturer name */
	free ( manufacturer );

	/* Free handle list */
	bs->FreePool ( targets );
}
