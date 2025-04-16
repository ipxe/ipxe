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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ipxe/version.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/DriverBinding.h>
#include <ipxe/efi/Protocol/ComponentName2.h>
#include <ipxe/efi/Protocol/DevicePath.h>
#include <ipxe/efi/efi_strings.h>
#include <ipxe/efi/efi_path.h>
#include <ipxe/efi/efi_driver.h>

/** @file
 *
 * EFI driver interface
 *
 */

/* Disambiguate the various error causes */
#define EINFO_EEFI_CONNECT						\
	__einfo_uniqify ( EINFO_EPLATFORM, 0x01,			\
			  "Could not connect controllers" )
#define EINFO_EEFI_CONNECT_PROHIBITED					\
	__einfo_platformify ( EINFO_EEFI_CONNECT,			\
			      EFI_SECURITY_VIOLATION,			\
			      "Connecting controllers prohibited by "	\
			      "security policy" )
#define EEFI_CONNECT_PROHIBITED						\
	__einfo_error ( EINFO_EEFI_CONNECT_PROHIBITED )
#define EEFI_CONNECT( efirc ) EPLATFORM ( EINFO_EEFI_CONNECT, efirc,	\
					  EEFI_CONNECT_PROHIBITED )

static EFI_DRIVER_BINDING_PROTOCOL efi_driver_binding;

/** List of controlled EFI devices */
static LIST_HEAD ( efi_devices );

/** We are currently disconnecting drivers */
static int efi_driver_disconnecting;

/**
 * Allocate new EFI device
 *
 * @v device		EFI device handle
 * @ret efidev		EFI device, or NULL on error
 */
struct efi_device * efidev_alloc ( EFI_HANDLE device ) {
	struct efi_device *efidev = NULL;
	EFI_DEVICE_PATH_PROTOCOL *path;
	EFI_DEVICE_PATH_PROTOCOL *path_end;
	size_t path_len;
	int rc;

	/* Open device path */
	if ( ( rc = efi_open ( device, &efi_device_path_protocol_guid,
			       &path ) ) != 0 ) {
		DBGC ( device, "EFIDRV %s could not open device path: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		return NULL;
	}
	path_len = ( efi_path_len ( path ) + sizeof ( *path_end ) );

	/* Allocate and initialise structure */
	efidev = zalloc ( sizeof ( *efidev ) + path_len );
	if ( ! efidev )
		return NULL;
	efidev->device = device;
	efidev->dev.desc.bus_type = BUS_TYPE_EFI;
	efidev->path = ( ( ( void * ) efidev ) + sizeof ( *efidev ) );
	memcpy ( efidev->path, path, path_len );
	INIT_LIST_HEAD ( &efidev->dev.children );
	list_add ( &efidev->dev.siblings, &efi_devices );

	return efidev;
}

/**
 * Free EFI device
 *
 * @v efidev		EFI device
 */
void efidev_free ( struct efi_device *efidev ) {

	assert ( list_empty ( &efidev->dev.children ) );
	list_del ( &efidev->dev.siblings );
	free ( efidev );
}

/**
 * Find EFI device
 *
 * @v device		EFI device handle (or child handle)
 * @ret efidev		EFI device, or NULL if not found
 */
static struct efi_device * efidev_find ( EFI_HANDLE device ) {
	struct efi_device *efidev;

	/* Avoid false positive matches against NULL children */
	if ( ! device )
		return NULL;

	/* Look for an existing EFI device */
	list_for_each_entry ( efidev, &efi_devices, dev.siblings ) {
		if ( ( device == efidev->device ) ||
		     ( device == efidev->child ) ) {
			return efidev;
		}
	}

	return NULL;
}

/**
 * Get parent EFI device
 *
 * @v dev		Generic device
 * @ret efidev		Parent EFI device, or NULL
 */
struct efi_device * efidev_parent ( struct device *dev ) {
	struct device *parent;
	struct efi_device *efidev;

	/* Walk upwards until we find a registered EFI device */
	while ( ( parent = dev->parent ) ) {
		list_for_each_entry ( efidev, &efi_devices, dev.siblings ) {
			if ( parent == &efidev->dev )
				return efidev;
		}
		dev = parent;
	}

	return NULL;
}

/**
 * Check to see if driver supports a device
 *
 * @v driver		EFI driver
 * @v device		EFI device
 * @v child		Path to child device, if any
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_driver_supported ( EFI_DRIVER_BINDING_PROTOCOL *driver __unused,
		       EFI_HANDLE device, EFI_DEVICE_PATH_PROTOCOL *child ) {
	struct efi_driver *efidrv;
	unsigned int count;
	int rc;

	DBGCP ( device, "EFIDRV %s DRIVER_SUPPORTED",
		efi_handle_name ( device ) );
	if ( child )
		DBGCP ( device, " (child %s)", efi_devpath_text ( child ) );
	DBGCP ( device, "\n" );

	/* Do nothing if we are already driving this device */
	if ( efidev_find ( device ) != NULL ) {
		DBGCP ( device, "EFIDRV %s is already started\n",
			efi_handle_name ( device ) );
		return EFI_ALREADY_STARTED;
	}

	/* Count drivers claiming to support this device */
	count = 0;
	for_each_table_entry ( efidrv, EFI_DRIVERS ) {
		if ( ( rc = efidrv->supported ( device ) ) == 0 ) {
			DBGC ( device, "EFIDRV %s has driver \"%s\"\n",
			       efi_handle_name ( device ), efidrv->name );
			count++;
		}
	}

	/* Check that we have at least one driver */
	if ( ! count ) {
		DBGCP ( device, "EFIDRV %s has no driver\n",
			efi_handle_name ( device ) );
		return EFI_UNSUPPORTED;
	}

	return 0;
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
efi_driver_start ( EFI_DRIVER_BINDING_PROTOCOL *driver __unused,
		   EFI_HANDLE device, EFI_DEVICE_PATH_PROTOCOL *child ) {
	struct efi_driver *efidrv;
	struct efi_device *efidev;
	struct efi_saved_tpl tpl;
	EFI_STATUS efirc;
	int rc;

	DBGC ( device, "EFIDRV %s DRIVER_START", efi_handle_name ( device ) );
	if ( child )
		DBGC ( device, " (child %s)", efi_devpath_text ( child ) );
	DBGC ( device, "\n" );

	/* Do nothing if we are already driving this device */
	efidev = efidev_find ( device );
	if ( efidev ) {
		DBGCP ( device, "EFIDRV %s is already started\n",
			efi_handle_name ( device ) );
		efirc = EFI_ALREADY_STARTED;
		goto err_already_started;
	}

	/* Raise TPL */
	efi_raise_tpl ( &tpl );

	/* Do nothing if we are currently disconnecting drivers */
	if ( efi_driver_disconnecting ) {
		DBGC ( device, "EFIDRV %s refusing to start during "
		       "disconnection\n", efi_handle_name ( device ) );
		efirc = EFI_NOT_READY;
		goto err_disconnecting;
	}

	/* Add new device */
	efidev = efidev_alloc ( device );
	if ( ! efidev ) {
		efirc = EFI_OUT_OF_RESOURCES;
		goto err_alloc;
	}

	/* Try to start this device */
	for_each_table_entry ( efidrv, EFI_DRIVERS ) {
		if ( ( rc = efidrv->supported ( device ) ) != 0 ) {
			DBGC ( device, "EFIDRV %s is not supported by driver "
			       "\"%s\": %s\n", efi_handle_name ( device ),
			       efidrv->name,
			       strerror ( rc ) );
			continue;
		}
		if ( ( rc = efidrv->start ( efidev ) ) == 0 ) {
			efidev->driver = efidrv;
			DBGC ( device, "EFIDRV %s using driver \"%s\"\n",
			       efi_handle_name ( device ),
			       efidev->driver->name );
			efi_restore_tpl ( &tpl );
			return 0;
		}
		DBGC ( device, "EFIDRV %s could not start driver \"%s\": %s\n",
		       efi_handle_name ( device ), efidrv->name,
		       strerror ( rc ) );
	}
	efirc = EFI_UNSUPPORTED;

	efidev_free ( efidev );
 err_alloc:
 err_disconnecting:
	efi_restore_tpl ( &tpl );
 err_already_started:
	return efirc;
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
efi_driver_stop ( EFI_DRIVER_BINDING_PROTOCOL *driver __unused,
		  EFI_HANDLE device, UINTN num_children,
		  EFI_HANDLE *children ) {
	struct efi_driver *efidrv;
	struct efi_device *efidev;
	struct efi_saved_tpl tpl;
	UINTN i;

	DBGC ( device, "EFIDRV %s DRIVER_STOP", efi_handle_name ( device ) );
	for ( i = 0 ; i < num_children ; i++ ) {
		DBGC ( device, "%s%s", ( i ? ", " : " child " ),
		       efi_handle_name ( children[i] ) );
	}
	DBGC ( device, "\n" );

	/* Do nothing unless we are driving this device */
	efidev = efidev_find ( device );
	if ( ! efidev ) {
		DBGCP ( device, "EFIDRV %s is not started\n",
			efi_handle_name ( device ) );
		return 0;
	}

	/* Raise TPL */
	efi_raise_tpl ( &tpl );

	/* Stop this device */
	efidrv = efidev->driver;
	assert ( efidrv != NULL );
	efidrv->stop ( efidev );
	efidev_free ( efidev );

	efi_restore_tpl ( &tpl );
	return 0;
}

/** EFI driver binding protocol */
static EFI_DRIVER_BINDING_PROTOCOL efi_driver_binding = {
	.Supported = efi_driver_supported,
	.Start = efi_driver_start,
	.Stop = efi_driver_stop,
};

/**
 * Look up driver name
 *
 * @v wtf		Component name protocol
 * @v language		Language to use
 * @v driver_name	Driver name to fill in
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_driver_name ( EFI_COMPONENT_NAME2_PROTOCOL *wtf __unused,
		  CHAR8 *language __unused, CHAR16 **driver_name ) {
	const wchar_t *name;

	name = ( product_wname[0] ? product_wname : build_wname );
	*driver_name = ( ( wchar_t * ) name );
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
efi_driver_controller_name ( EFI_COMPONENT_NAME2_PROTOCOL *wtf __unused,
			     EFI_HANDLE device, EFI_HANDLE child,
			     CHAR8 *language, CHAR16 **controller_name ) {
	EFI_COMPONENT_NAME2_PROTOCOL *name2;
	int rc;

	/* Delegate to the EFI_COMPONENT_NAME2_PROTOCOL instance
	 * installed on child handle, if present.
	 */
	if ( ( child != NULL ) &&
	     ( ( rc = efi_open ( child, &efi_component_name2_protocol_guid,
				 &name2 ) ) == 0 ) ) {
		return name2->GetControllerName ( name2, device, child,
						  language, controller_name );
	}

	/* Otherwise, let EFI use the default Device Path Name */
	return EFI_UNSUPPORTED;
}

/** EFI component name protocol */
static EFI_COMPONENT_NAME2_PROTOCOL efi_wtf = {
	.GetDriverName = efi_driver_name,
	.GetControllerName = efi_driver_controller_name,
	.SupportedLanguages = "en",
};

/**
 * Install EFI driver
 *
 * @ret rc		Return status code
 */
int efi_driver_install ( void ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_STATUS efirc;
	int rc;

	/* Calculate driver version number.  We use the build
	 * timestamp (in seconds since the Epoch) shifted right by six
	 * bits: this gives us an approximately one-minute resolution
	 * and a scheme which will last until the year 10680.
	 */
	efi_driver_binding.Version = ( build_timestamp >> 6 );

	/* Install protocols on image handle */
	efi_driver_binding.ImageHandle = efi_image_handle;
	efi_driver_binding.DriverBindingHandle = efi_image_handle;
	if ( ( efirc = bs->InstallMultipleProtocolInterfaces (
			&efi_image_handle,
			&efi_driver_binding_protocol_guid, &efi_driver_binding,
			&efi_component_name2_protocol_guid, &efi_wtf,
			NULL ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( &efi_driver_binding, "EFIDRV could not install "
		       "protocols: %s\n", strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Uninstall EFI driver
 *
 */
void efi_driver_uninstall ( void ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;

	/* Uninstall protocols */
	bs->UninstallMultipleProtocolInterfaces (
		efi_image_handle,
		&efi_driver_binding_protocol_guid, &efi_driver_binding,
		&efi_component_name2_protocol_guid, &efi_wtf, NULL );
}

/**
 * Try to disconnect an existing EFI driver
 *
 * @v device		EFI device
 * @v protocol		Protocol GUID
 * @ret rc		Return status code
 */
int efi_driver_exclude ( EFI_HANDLE device, EFI_GUID *protocol ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *openers;
	EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *opener;
	EFI_HANDLE driver;
	UINTN count;
	unsigned int i;
	EFI_STATUS efirc;
	int rc;

	/* Retrieve list of openers */
	if ( ( efirc = bs->OpenProtocolInformation ( device, protocol, &openers,
						     &count ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( device, "EFIDRV %s could not list %s openers: %s\n",
		       efi_handle_name ( device ), efi_guid_ntoa ( protocol ),
		       strerror ( rc ) );
		goto err_list;
	}

	/* Identify BY_DRIVER opener */
	driver = NULL;
	for ( i = 0 ; i < count ; i++ ) {
		opener = &openers[i];
		if ( opener->Attributes & EFI_OPEN_PROTOCOL_BY_DRIVER ) {
			driver = opener->AgentHandle;
			break;
		}
	}

	/* Try to disconnect driver */
	DBGC ( device, "EFIDRV %s disconnecting %s drivers\n",
	       efi_handle_name ( device ), efi_guid_ntoa ( protocol ) );
	if ( driver ) {
		DBGC ( device, "EFIDRV %s disconnecting %s driver ",
		       efi_handle_name ( device ), efi_guid_ntoa ( protocol ) );
		DBGC ( device, "%s\n", efi_handle_name ( driver ) );
		if ( ( efirc = bs->DisconnectController ( device, driver,
							  NULL ) ) != 0 ) {
			rc = -EEFI ( efirc );
			DBGC ( device, "EFIDRV %s could not disconnect ",
			       efi_handle_name ( device ) );
			DBGC ( device, "%s: %s\n",
			       efi_handle_name ( driver ), strerror ( rc ) );
			goto err_disconnect;
		}
	}

	/* Success */
	rc = 0;

 err_disconnect:
	bs->FreePool ( openers );
 err_list:
	return rc;
}

/**
 * Try to connect EFI driver
 *
 * @v device		EFI device
 * @ret rc		Return status code
 */
static int efi_driver_connect ( EFI_HANDLE device ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE drivers[2] =
		{ efi_driver_binding.DriverBindingHandle, NULL };
	struct efi_driver *efidrv;
	EFI_STATUS efirc;
	int rc;

	/* Check if we want to drive this device */
	if ( ( efirc = efi_driver_supported ( &efi_driver_binding, device,
					      NULL ) ) != 0 ) {
		/* Not supported; not an error */
		return 0;
	}

	/* Disconnect any existing drivers */
	DBGC2 ( device, "EFIDRV %s before disconnecting:\n",
		efi_handle_name ( device ) );
	DBGC2_EFI_PROTOCOLS ( device, device );
	DBGC ( device, "EFIDRV %s disconnecting existing drivers\n",
	       efi_handle_name ( device ) );
	efi_driver_disconnecting = 1;
	for_each_table_entry_reverse ( efidrv, EFI_DRIVERS ) {
		if ( ! efidrv->exclude )
			continue;
		if ( ( rc = efidrv->supported ( device ) ) != 0 )
			continue;
		if ( ( rc = efidrv->exclude ( device ) ) != 0 ) {
			DBGC ( device, "EFIDRV %s could not disconnect "
			       "drivers: %s\n", efi_handle_name ( device ),
			       strerror ( rc ) );
			/* Ignore the error and attempt to connect anyway */
		}
	}
	efi_driver_disconnecting = 0;
	DBGC2 ( device, "EFIDRV %s after disconnecting:\n",
		efi_handle_name ( device ) );
	DBGC2_EFI_PROTOCOLS ( device, device );

	/* Connect our driver */
	DBGC ( device, "EFIDRV %s connecting new drivers\n",
	       efi_handle_name ( device ) );
	if ( ( efirc = bs->ConnectController ( device, drivers, NULL,
					       TRUE ) ) != 0 ) {
		rc = -EEFI_CONNECT ( efirc );
		DBGC ( device, "EFIDRV %s could not connect new drivers: "
		       "%s\n", efi_handle_name ( device ), strerror ( rc ) );
		DBGC ( device, "EFIDRV %s connecting driver directly\n",
		       efi_handle_name ( device ) );
		if ( ( efirc = efi_driver_start ( &efi_driver_binding, device,
						  NULL ) ) != 0 ) {
			rc = -EEFI_CONNECT ( efirc );
			DBGC ( device, "EFIDRV %s could not connect driver "
			       "directly: %s\n", efi_handle_name ( device ),
			       strerror ( rc ) );
			return rc;
		}
	}
	DBGC2 ( device, "EFIDRV %s after connecting:\n",
		efi_handle_name ( device ) );
	DBGC2_EFI_PROTOCOLS ( device, device );

	return 0;
}

/**
 * Try to disconnect EFI driver
 *
 * @v device		EFI device
 * @ret rc		Return status code
 */
static int efi_driver_disconnect ( EFI_HANDLE device ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;

	/* Disconnect our driver */
	efi_driver_disconnecting = 1;
	bs->DisconnectController ( device,
				   efi_driver_binding.DriverBindingHandle,
				   NULL );
	efi_driver_disconnecting = 0;
	return 0;
}

/**
 * Reconnect original EFI driver
 *
 * @v device		EFI device
 * @ret rc		Return status code
 */
static int efi_driver_reconnect ( EFI_HANDLE device ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;

	/* Reconnect any available driver */
	bs->ConnectController ( device, NULL, NULL, TRUE );

	return 0;
}

/**
 * Connect/disconnect EFI driver from all handles
 *
 * @v method		Connect/disconnect method
 * @ret rc		Return status code
 */
static int efi_driver_handles ( int ( * method ) ( EFI_HANDLE handle ) ) {
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
		DBGC ( &efi_driver_binding, "EFIDRV could not list handles: "
		       "%s\n", strerror ( rc ) );
		goto err_locate;
	}

	/* Connect/disconnect driver from all handles */
	for ( i = 0 ; i < num_handles ; i++ ) {
		if ( ( rc = method ( handles[i] ) ) != 0 ) {
			/* Ignore errors and continue to process
			 * remaining handles.
			 */
		}
	}

	/* Success */
	rc = 0;

	bs->FreePool ( handles );
 err_locate:
	return rc;
}

/**
 * Connect EFI driver to all possible devices
 *
 * @ret rc		Return status code
 */
int efi_driver_connect_all ( void ) {

	DBGC ( &efi_driver_binding, "EFIDRV connecting our drivers\n" );
	return efi_driver_handles ( efi_driver_connect );
}

/**
 * Disconnect EFI driver from all possible devices
 *
 * @ret rc		Return status code
 */
void efi_driver_disconnect_all ( void ) {

	DBGC ( &efi_driver_binding, "EFIDRV disconnecting our drivers\n" );
	efi_driver_handles ( efi_driver_disconnect );
}

/**
 * Reconnect original EFI drivers to all possible devices
 *
 * @ret rc		Return status code
 */
void efi_driver_reconnect_all ( void ) {

	DBGC ( &efi_driver_binding, "EFIDRV reconnecting old drivers\n" );
	efi_driver_handles ( efi_driver_reconnect );
}
