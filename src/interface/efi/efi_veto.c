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
#include <ipxe/efi/Protocol/ComponentName2.h>
#include <ipxe/efi/efi_veto.h>

/** @file
 *
 * EFI driver vetoes
 *
 */

/** A driver veto candidate */
struct efi_veto_candidate {
	/** Veto name (for debugging) */
	const char *name;
	/**
	 * Check if driver is vetoed
	 *
	 * @v binding		Driver binding protocol
	 * @v loaded		Loaded image protocol
	 * @v manufacturer	Manufacturer name, if present
	 * @v name		Driver name, if present
	 * @ret vetoed		Driver is to be vetoed
	 */
	int ( * veto ) ( EFI_DRIVER_BINDING_PROTOCOL *binding,
			 EFI_LOADED_IMAGE_PROTOCOL *loaded,
			 const char *manufacturer, const CHAR16 *name );
};

/** A driver veto */
struct efi_veto {
	/** Driver binding handle */
	EFI_HANDLE driver;
	/** Driving binding protocol */
	EFI_DRIVER_BINDING_PROTOCOL *binding;
	/** Image handle */
	EFI_HANDLE image;
	/** Loaded image protocol */
	EFI_LOADED_IMAGE_PROTOCOL *loaded;
};

/**
 * Unload an EFI driver
 *
 * @v veto		Driver veto
 * @ret rc		Return status code
 */
static int efi_veto_unload ( struct efi_veto *veto ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE driver = veto->driver;
	EFI_HANDLE image = veto->image;
	EFI_STATUS efirc;
	int rc;

	/* Unload the driver */
	if ( ( efirc = bs->UnloadImage ( image ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( driver, "EFIVETO %s could not unload",
		       efi_handle_name ( driver ) );
		DBGC ( driver, " %s: %s\n", efi_handle_name ( image ),
		       strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Disconnect an EFI driver from all handles
 *
 * @v veto		Driver veto
 * @ret rc		Return status code
 */
static int efi_veto_disconnect ( struct efi_veto *veto ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE driver = veto->driver;
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
 * @v veto		Driver veto
 * @ret rc		Return status code
 */
static int efi_veto_uninstall ( struct efi_veto *veto ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE driver = veto->driver;
	EFI_DRIVER_BINDING_PROTOCOL *binding;
	EFI_STATUS efirc;
	int rc;

	/* Open driver binding protocol */
	if ( ( rc = efi_open ( driver, &efi_driver_binding_protocol_guid,
			       &binding ) ) != 0 ) {
		DBGC ( driver, "EFIVETO %s could not open driver binding "
		       "protocol: %s\n", efi_handle_name ( driver ),
		       strerror ( rc ) );
		return rc;
	}

	/* Uninstall driver binding protocol */
	if ( ( efirc = bs->UninstallMultipleProtocolInterfaces (
			driver, &efi_driver_binding_protocol_guid,
			binding, NULL ) ) != 0 ) {
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
 * @v veto		Driver veto
 * @v handle		Potentially opened handle
 * @v protocol		Opened protocol
 * @ret rc		Return status code
 */
static int efi_veto_close_protocol ( struct efi_veto *veto, EFI_HANDLE handle,
				     EFI_GUID *protocol ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE driver = veto->driver;
	EFI_HANDLE image = veto->image;
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
		opener = &openers[ count - i - 1 ];
		if ( ( opener->AgentHandle != driver ) &&
		     ( opener->AgentHandle != image ) ) {
			continue;
		}
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
 * @v veto		Driver veto
 * @v handle		Potentially opened handle
 * @ret rc		Return status code
 */
static int efi_veto_close_handle ( struct efi_veto *veto, EFI_HANDLE handle ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE driver = veto->driver;
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
		DBGC ( driver, "EFIVETO %s could not retrieve protocols",
		       efi_handle_name ( driver ) );
		DBGC ( driver, " for %s: %s\n",
		       efi_handle_name ( handle ), strerror ( rc ) );
		goto err_list;
	}

	/* Close each protocol */
	for ( i = 0 ; i < count ; i++ ) {
		protocol = protocols[ count - i - 1];
		if ( ( rc = efi_veto_close_protocol ( veto, handle,
						      protocol ) ) != 0 )
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
 * @v veto		Driver veto
 * @ret rc		Return status code
 */
static int efi_veto_close ( struct efi_veto *veto ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE driver = veto->driver;
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

	/* Close each handle */
	for ( i = 0 ; i < count ; i++ ) {
		handle = handles[ count - i - 1 ];
		if ( ( rc = efi_veto_close_handle ( veto, handle ) ) != 0 )
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
 * @v veto		Driver veto
 * @ret rc		Return status code
 */
static int efi_veto_destroy ( struct efi_veto *veto ) {
	EFI_HANDLE driver = veto->driver;
	int rc;

	/* Disconnect driver from all handles */
	if ( ( rc = efi_veto_disconnect ( veto ) ) != 0 )
		return rc;

	/* Uninstall driver binding protocol */
	if ( ( rc = efi_veto_uninstall ( veto ) ) != 0 )
		return rc;

	/* Close any remaining opened handles */
	if ( ( rc = efi_veto_close ( veto ) ) != 0 )
		return rc;

	DBGC ( driver, "EFIVETO %s forcibly removed\n",
	       efi_handle_name ( driver ) );
	return 0;
}

/**
 * Veto an EFI driver
 *
 * @v veto		Driver veto
 * @ret rc		Return status code
 */
static int efi_veto_driver ( struct efi_veto *veto ) {
	int rc;

	/* Try gracefully unloading the driver */
	if ( ( rc = efi_veto_unload ( veto ) ) == 0 )
		return 0;

	/* If that fails, use a hammer */
	if ( ( rc = efi_veto_destroy ( veto ) ) == 0 )
		return 0;

	return rc;
}

/**
 * Veto Ip4ConfigDxe driver on some platforms
 *
 * @v binding		Driver binding protocol
 * @v loaded		Loaded image protocol
 * @v manufacturer	Manufacturer name, if present
 * @v name		Driver name, if present
 * @ret vetoed		Driver is to be vetoed
 */
static int
efi_veto_ip4config ( EFI_DRIVER_BINDING_PROTOCOL *binding __unused,
		     EFI_LOADED_IMAGE_PROTOCOL *loaded __unused,
		     const char *manufacturer, const CHAR16 *name ) {
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
 * @v binding		Driver binding protocol
 * @v loaded		Loaded image protocol
 * @v manufacturer	Manufacturer name, if present
 * @v name		Driver name, if present
 * @ret vetoed		Driver is to be vetoed
 */
static int
efi_veto_hp_xhci ( EFI_DRIVER_BINDING_PROTOCOL *binding __unused,
		   EFI_LOADED_IMAGE_PROTOCOL *loaded __unused,
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

/**
 * Veto VMware UefiPxeBcDxe driver
 *
 * @v binding		Driver binding protocol
 * @v loaded		Loaded image protocol
 * @v manufacturer	Manufacturer name, if present
 * @v name		Driver name, if present
 * @ret vetoed		Driver is to be vetoed
 */
static int
efi_veto_vmware_uefipxebc ( EFI_DRIVER_BINDING_PROTOCOL *binding __unused,
			    EFI_LOADED_IMAGE_PROTOCOL *loaded __unused,
			    const char *manufacturer, const CHAR16 *name ) {
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
 * Veto Dhcp6Dxe driver
 *
 * @v binding		Driver binding protocol
 * @v loaded		Loaded image protocol
 * @v manufacturer	Manufacturer name, if present
 * @v name		Driver name, if present
 * @ret vetoed		Driver is to be vetoed
 */
static int efi_veto_dhcp6 ( EFI_DRIVER_BINDING_PROTOCOL *binding __unused,
			    EFI_LOADED_IMAGE_PROTOCOL *loaded __unused,
			    const char *manufacturer __unused,
			    const CHAR16 *name ) {
	static const CHAR16 dhcp6[] = L"DHCP6 Protocol Driver";

	/* Check driver name */
	if ( ! name )
		return 0;
	if ( memcmp ( name, dhcp6, sizeof ( dhcp6 ) ) != 0 )
		return 0;

	return 1;
}

/** Driver vetoes */
static struct efi_veto_candidate efi_vetoes[] = {
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
		.name = "Dhcp6",
		.veto = efi_veto_dhcp6,
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
			   struct efi_veto *veto ) {
	EFI_DRIVER_BINDING_PROTOCOL *binding;
	EFI_LOADED_IMAGE_PROTOCOL *loaded;
	EFI_COMPONENT_NAME2_PROTOCOL *wtf2;
	EFI_COMPONENT_NAME_PROTOCOL *wtf;
	CHAR16 *name;
	unsigned int i;
	EFI_HANDLE image;
	EFI_STATUS efirc;
	int rc;

	/* Mark as not vetoed */
	memset ( veto, 0, sizeof ( *veto ) );

	/* Open driver binding protocol */
	if ( ( rc = efi_open ( driver, &efi_driver_binding_protocol_guid,
			       &binding ) ) != 0 ) {
		DBGC ( driver, "EFIVETO %s could not open driver binding "
		       "protocol: %s\n", efi_handle_name ( driver ),
		       strerror ( rc ) );
		return rc;
	}
	image = binding->ImageHandle;

	/* Open loaded image protocol */
	if ( ( rc = efi_open ( image, &efi_loaded_image_protocol_guid,
			       &loaded ) ) != 0 ) {
		DBGC ( driver, "EFIVETO %s could not open",
		       efi_handle_name ( driver ) );
		DBGC ( driver, " %s loaded image protocol: %s\n",
		       efi_handle_name ( image ), strerror ( rc ) );
		return rc;
	}

	/* Open component name protocol, if present */
	if ( ( rc = efi_open ( image, &efi_component_name2_protocol_guid,
			       &wtf2 ) ) != 0 ) {
		/* Ignore failure; is not required to be present */
	}

	/* Open obsolete component name protocol, if present */
	if ( ( rc = efi_open ( image, &efi_component_name_protocol_guid,
			       &wtf ) ) != 0 ) {
		/* Ignore failure; is not required to be present */
	}

	/* Get driver name, if available */
	if ( ( wtf2 && ( ( efirc = wtf2->GetDriverName ( wtf2, "en",
							 &name ) == 0 ) ) ) ||
	     ( wtf && ( ( efirc = wtf->GetDriverName ( wtf, "eng",
						       &name ) == 0 ) ) ) ) {
		/* Driver has a name */
	} else {
		/* Ignore failure; name is not required to be present */
		name = NULL;
	}

	/* Check vetoes */
	DBGC2 ( &efi_vetoes, "EFIVETO checking %s [%p,%p)\n",
		efi_handle_name ( driver ), loaded->ImageBase,
		( loaded->ImageBase + loaded->ImageSize ) );
	for ( i = 0 ; i < ( sizeof ( efi_vetoes ) /
			    sizeof ( efi_vetoes[0] ) ) ; i++ ) {
		if ( efi_vetoes[i].veto ( binding, loaded, manufacturer,
					  name ) ) {
			DBGC ( driver, "EFIVETO %s is vetoed (%s)\n",
			       efi_handle_name ( driver ),
			       efi_vetoes[i].name );
			veto->driver = driver;
			veto->binding = binding;
			veto->image = image;
			veto->loaded = loaded;
			break;
		}
	}

	return 0;
}

/**
 * Remove any vetoed drivers
 *
 */
void efi_veto ( void ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct efi_veto veto;
	EFI_HANDLE *drivers;
	EFI_HANDLE driver;
	UINTN count;
	unsigned int i;
	char *manufacturer;
	EFI_STATUS efirc;
	int rc;

	/* Locate all driver binding protocol handles */
	if ( ( efirc = bs->LocateHandleBuffer (
			ByProtocol, &efi_driver_binding_protocol_guid,
			NULL, &count, &drivers ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( &efi_vetoes, "EFIVETO could not list all drivers: "
		       "%s\n", strerror ( rc ) );
		return;
	}

	/* Get manufacturer name */
	fetch_string_setting_copy ( NULL, &manufacturer_setting,
				    &manufacturer );
	DBGC ( &efi_vetoes, "EFIVETO manufacturer is \"%s\"\n", manufacturer );

	/* Unload any vetoed drivers */
	for ( i = 0 ; i < count ; i++ ) {
		driver = drivers[ count - i - 1 ];
		if ( ( rc = efi_veto_find ( driver, manufacturer,
					    &veto ) ) != 0 ) {
			DBGC ( driver, "EFIVETO %s could not determine "
			       "vetoing: %s\n",
			       efi_handle_name ( driver ), strerror ( rc ) );
			continue;
		}
		if ( ! veto.driver )
			continue;
		if ( ( rc = efi_veto_driver ( &veto ) ) != 0 ) {
			DBGC ( driver, "EFIVETO %s could not veto: %s\n",
			       efi_handle_name ( driver ), strerror ( rc ) );
		}
	}

	/* Free manufacturer name */
	free ( manufacturer );

	/* Free handle list */
	bs->FreePool ( drivers );
}
