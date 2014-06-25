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

#include <stdlib.h>
#include <errno.h>
#include <ipxe/pci.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_pci.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/Protocol/PciIo.h>
#include <ipxe/efi/Protocol/PciRootBridgeIo.h>

/** @file
 *
 * iPXE PCI I/O API for EFI
 *
 */

/* Disambiguate the various error causes */
#define EINFO_EEFI_PCI							\
	__einfo_uniqify ( EINFO_EPLATFORM, 0x01,			\
			  "Could not open PCI I/O protocols" )
#define EINFO_EEFI_PCI_NOT_PCI						\
	__einfo_platformify ( EINFO_EEFI_PCI, EFI_UNSUPPORTED,		\
			      "Not a PCI device" )
#define EEFI_PCI_NOT_PCI __einfo_error ( EINFO_EEFI_PCI_NOT_PCI )
#define EINFO_EEFI_PCI_IN_USE						\
	__einfo_platformify ( EINFO_EEFI_PCI, EFI_ACCESS_DENIED,	\
			      "PCI device already has a driver" )
#define EEFI_PCI_IN_USE __einfo_error ( EINFO_EEFI_PCI_IN_USE )
#define EEFI_PCI( efirc )						\
	EPLATFORM ( EINFO_EEFI_PCI, efirc,				\
		    EEFI_PCI_NOT_PCI, EEFI_PCI_IN_USE )

/******************************************************************************
 *
 * iPXE PCI API
 *
 ******************************************************************************
 */

/** PCI root bridge I/O protocol */
static EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *efipci;
EFI_REQUIRE_PROTOCOL ( EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL, &efipci );

static unsigned long efipci_address ( struct pci_device *pci,
				      unsigned long location ) {
	return EFI_PCI_ADDRESS ( PCI_BUS ( pci->busdevfn ),
				 PCI_SLOT ( pci->busdevfn ),
				 PCI_FUNC ( pci->busdevfn ),
				 EFIPCI_OFFSET ( location ) );
}

int efipci_read ( struct pci_device *pci, unsigned long location,
		  void *value ) {
	EFI_STATUS efirc;
	int rc;

	if ( ( efirc = efipci->Pci.Read ( efipci, EFIPCI_WIDTH ( location ),
					  efipci_address ( pci, location ), 1,
					  value ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBG ( "EFIPCI config read from " PCI_FMT " offset %02lx "
		      "failed: %s\n", PCI_ARGS ( pci ),
		      EFIPCI_OFFSET ( location ), strerror ( rc ) );
		return -EIO;
	}

	return 0;
}

int efipci_write ( struct pci_device *pci, unsigned long location,
		   unsigned long value ) {
	EFI_STATUS efirc;
	int rc;

	if ( ( efirc = efipci->Pci.Write ( efipci, EFIPCI_WIDTH ( location ),
					   efipci_address ( pci, location ), 1,
					   &value ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBG ( "EFIPCI config write to " PCI_FMT " offset %02lx "
		      "failed: %s\n", PCI_ARGS ( pci ),
		      EFIPCI_OFFSET ( location ), strerror ( rc ) );
		return -EIO;
	}

	return 0;
}

PROVIDE_PCIAPI_INLINE ( efi, pci_num_bus );
PROVIDE_PCIAPI_INLINE ( efi, pci_read_config_byte );
PROVIDE_PCIAPI_INLINE ( efi, pci_read_config_word );
PROVIDE_PCIAPI_INLINE ( efi, pci_read_config_dword );
PROVIDE_PCIAPI_INLINE ( efi, pci_write_config_byte );
PROVIDE_PCIAPI_INLINE ( efi, pci_write_config_word );
PROVIDE_PCIAPI_INLINE ( efi, pci_write_config_dword );

/******************************************************************************
 *
 * EFI PCI device instantiation
 *
 ******************************************************************************
 */

/** EFI PCI I/O protocol GUID */
static EFI_GUID efi_pci_io_protocol_guid
	= EFI_PCI_IO_PROTOCOL_GUID;

/** EFI device path protocol GUID */
static EFI_GUID efi_device_path_protocol_guid
	= EFI_DEVICE_PATH_PROTOCOL_GUID;

/** EFI PCI devices */
static LIST_HEAD ( efi_pci_devices );

/**
 * Create EFI PCI device
 *
 * @v device		EFI device
 * @v attributes	Protocol opening attributes
 * @v efipci		EFI PCI device to fill in
 * @ret rc		Return status code
 */
int efipci_create ( EFI_HANDLE device, UINT32 attributes,
		    struct efi_pci_device **efipci ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	union {
		EFI_PCI_IO_PROTOCOL *pci_io;
		void *interface;
	} pci_io;
	union {
		EFI_DEVICE_PATH_PROTOCOL *path;
		void *interface;
	} path;
	UINTN pci_segment, pci_bus, pci_dev, pci_fn;
	EFI_STATUS efirc;
	int rc;

	/* Allocate PCI device */
	*efipci = zalloc ( sizeof ( **efipci ) );
	if ( ! *efipci ) {
		rc = -ENOMEM;
		goto err_zalloc;
	}
	(*efipci)->device = device;

	/* See if device is a PCI device */
	if ( ( efirc = bs->OpenProtocol ( device,
					  &efi_pci_io_protocol_guid,
					  &pci_io.interface,
					  efi_image_handle,
					  device, attributes ) ) != 0 ) {
		rc = -EEFI_PCI ( efirc );
		DBGCP ( device, "EFIPCI %p %s cannot open PCI protocols: %s\n",
			device, efi_handle_devpath_text ( device ),
			strerror ( rc ) );
		goto err_open_protocol;
	}
	(*efipci)->pci_io = pci_io.pci_io;

	/* Get PCI bus:dev.fn address */
	if ( ( efirc = pci_io.pci_io->GetLocation ( pci_io.pci_io,
						    &pci_segment,
						    &pci_bus, &pci_dev,
						    &pci_fn ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( device, "EFIPCI %p %s could not get PCI location: "
		       "%s\n", device, efi_handle_devpath_text ( device ),
		       strerror ( rc ) );
		goto err_get_location;
	}
	DBGC2 ( device, "EFIPCI %p %s is PCI %04lx:%02lx:%02lx.%lx\n",
		device, efi_handle_devpath_text ( device ),
		( ( unsigned long ) pci_segment ), ( ( unsigned long ) pci_bus),
		( ( unsigned long ) pci_dev ), ( ( unsigned long ) pci_fn ) );

	/* Populate PCI device */
	pci_init ( &(*efipci)->pci, PCI_BUSDEVFN ( pci_bus, pci_dev, pci_fn ) );
	if ( ( rc = pci_read_config ( &(*efipci)->pci ) ) != 0 ) {
		DBGC ( device, "EFIPCI %p %s cannot read PCI configuration: "
		       "%s\n", device, efi_handle_devpath_text ( device ),
		       strerror ( rc ) );
		goto err_pci_read_config;
	}

	/* Retrieve device path */
	if ( ( efirc = bs->OpenProtocol ( device,
					  &efi_device_path_protocol_guid,
					  &path.interface, efi_image_handle,
					  device, attributes ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( device, "EFIPCI %p %s has no device path\n",
		       device, efi_handle_devpath_text ( device ) );
		goto err_no_device_path;
	}
	(*efipci)->path = path.path;

	/* Add to list of PCI devices */
	list_add ( &(*efipci)->list, &efi_pci_devices );

	return 0;

	bs->CloseProtocol ( device, &efi_device_path_protocol_guid,
			    efi_image_handle, device );
 err_no_device_path:
 err_pci_read_config:
 err_get_location:
	bs->CloseProtocol ( device, &efi_pci_io_protocol_guid,
			    efi_image_handle, device );
 err_open_protocol:
	free ( *efipci );
 err_zalloc:
	return rc;
}

/**
 * Enable EFI PCI device
 *
 * @v efipci		EFI PCI device
 * @ret rc		Return status code
 */
int efipci_enable ( struct efi_pci_device *efipci ) {
	EFI_PCI_IO_PROTOCOL *pci_io = efipci->pci_io;

	/* Try to enable I/O cycles, memory cycles, and bus mastering.
	 * Some platforms will 'helpfully' report errors if these bits
	 * can't be enabled (for example, if the card doesn't actually
	 * support I/O cycles).  Work around any such platforms by
	 * enabling bits individually and simply ignoring any errors.
	 */
	pci_io->Attributes ( pci_io, EfiPciIoAttributeOperationEnable,
			     EFI_PCI_IO_ATTRIBUTE_IO, NULL );
	pci_io->Attributes ( pci_io, EfiPciIoAttributeOperationEnable,
			     EFI_PCI_IO_ATTRIBUTE_MEMORY, NULL );
	pci_io->Attributes ( pci_io, EfiPciIoAttributeOperationEnable,
			     EFI_PCI_IO_ATTRIBUTE_BUS_MASTER, NULL );

	return 0;
}

/**
 * Find EFI PCI device by EFI device
 *
 * @v device		EFI device
 * @ret efipci		EFI PCI device, or NULL
 */
struct efi_pci_device * efipci_find_efi ( EFI_HANDLE device ) {
	struct efi_pci_device *efipci;

	list_for_each_entry ( efipci, &efi_pci_devices, list ) {
		if ( efipci->device == device )
			return efipci;
	}
	return NULL;
}

/**
 * Find EFI PCI device by iPXE device
 *
 * @v dev		Device
 * @ret efipci		EFI PCI device, or NULL
 */
struct efi_pci_device * efipci_find ( struct device *dev ) {
	struct efi_pci_device *efipci;

	list_for_each_entry ( efipci, &efi_pci_devices, list ) {
		if ( &efipci->pci.dev == dev )
			return efipci;
	}
	return NULL;
}

/**
 * Add EFI device as child of EFI PCI device
 *
 * @v efipci		EFI PCI device
 * @v device		EFI child device
 * @ret efirc		EFI status code
 */
int efipci_child_add ( struct efi_pci_device *efipci, EFI_HANDLE device ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	union {
		EFI_PCI_IO_PROTOCOL *pci_io;
		void *interface;
	} pci_io;
	EFI_STATUS efirc;
	int rc;

	/* Re-open the PCI_IO_PROTOCOL */
	if ( ( efirc = bs->OpenProtocol ( efipci->device,
					  &efi_pci_io_protocol_guid,
					  &pci_io.interface,
					  efi_image_handle, device,
					  EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
					  ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( efipci->device, "EFIPCI %p %s could not add child",
		       efipci->device, efi_devpath_text ( efipci->path ) );
		DBGC ( efipci->device, " %p %s: %s\n", device,
		       efi_handle_devpath_text ( device ), strerror ( rc ) );
		return rc;
	}

	DBGC2 ( efipci->device, "EFIPCI %p %s added child",
		efipci->device, efi_devpath_text ( efipci->path ) );
	DBGC2 ( efipci->device, " %p %s\n",
		device, efi_handle_devpath_text ( device ) );
	return 0;
}

/**
 * Remove EFI device as child of PCI device
 *
 * @v efipci		EFI PCI device
 * @v device		EFI child device
 * @ret efirc		EFI status code
 */
void efipci_child_del ( struct efi_pci_device *efipci, EFI_HANDLE device ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;

	bs->CloseProtocol ( efipci->device, &efi_pci_io_protocol_guid,
			    efi_image_handle, device );
	DBGC2 ( efipci->device, "EFIPCI %p %s removed child",
		efipci->device, efi_devpath_text ( efipci->path ) );
	DBGC2 ( efipci->device, " %p %s\n",
		device, efi_handle_devpath_text ( device ) );
}

/**
 * Destroy EFI PCI device
 *
 * @v efipci		EFI PCI device
 */
void efipci_destroy ( struct efi_pci_device *efipci ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;

	list_del ( &efipci->list );
	bs->CloseProtocol ( efipci->device, &efi_device_path_protocol_guid,
			    efi_image_handle, efipci->device );
	bs->CloseProtocol ( efipci->device, &efi_pci_io_protocol_guid,
			    efi_image_handle, efipci->device );
	free ( efipci );
}

/******************************************************************************
 *
 * EFI PCI driver
 *
 ******************************************************************************
 */

/**
 * Check to see if driver supports a device
 *
 * @v device		EFI device
 * @ret rc		Return status code
 */
static int efipci_supported ( EFI_HANDLE device ) {
	struct efi_pci_device *efipci;
	int rc;

	/* Do nothing if we are already driving this device */
	efipci = efipci_find_efi ( device );
	if ( efipci ) {
		DBGCP ( device, "EFIPCI %p %s already started\n",
			device, efi_devpath_text ( efipci->path ) );
		rc = -EALREADY;
		goto err_already_started;
	}

	/* Create temporary corresponding PCI device, if any */
	if ( ( rc = efipci_create ( device, EFI_OPEN_PROTOCOL_GET_PROTOCOL,
				    &efipci ) ) != 0 )
		goto err_create;

	/* Look for a driver */
	if ( ( rc = pci_find_driver ( &efipci->pci ) ) != 0 ) {
		DBGCP ( device, "EFIPCI %p %s has no driver\n",
			device, efi_devpath_text ( efipci->path ) );
		goto err_no_driver;
	}

	DBGC ( device, "EFIPCI %p %s has driver \"%s\"\n", device,
	       efi_devpath_text ( efipci->path ), efipci->pci.id->name );

	/* Destroy temporary PCI device */
	efipci_destroy ( efipci );

	return 0;

 err_no_driver:
	efipci_destroy ( efipci );
 err_create:
 err_already_started:
	return rc;
}

/**
 * Attach driver to device
 *
 * @v device		EFI device
 * @ret rc		Return status code
 */
static int efipci_start ( EFI_HANDLE device ) {
	struct efi_pci_device *efipci;
	int rc;

	/* Do nothing if we are already driving this device */
	efipci = efipci_find_efi ( device );
	if ( efipci ) {
		DBGCP ( device, "EFIPCI %p %s already started\n",
			device, efi_devpath_text ( efipci->path ) );
		rc = -EALREADY;
		goto err_already_started;
	}

	/* Create corresponding PCI device */
	if ( ( rc = efipci_create ( device, ( EFI_OPEN_PROTOCOL_BY_DRIVER |
					      EFI_OPEN_PROTOCOL_EXCLUSIVE ),
				    &efipci ) ) != 0 )
		goto err_create;

	/* Find driver */
	if ( ( rc = pci_find_driver ( &efipci->pci ) ) != 0 ) {
		DBGC ( device, "EFIPCI %p %s has no driver\n",
		       device, efi_devpath_text ( efipci->path ) );
		goto err_find_driver;
	}

	/* Enable PCI device */
	if ( ( rc = efipci_enable ( efipci ) ) != 0 )
		goto err_enable;

	/* Probe driver */
	if ( ( rc = pci_probe ( &efipci->pci ) ) != 0 ) {
		DBGC ( device, "EFIPCI %p %s could not probe driver \"%s\": "
		       "%s\n", device, efi_devpath_text ( efipci->path ),
		       efipci->pci.id->name, strerror ( rc ) );
		goto err_probe;
	}
	DBGC ( device, "EFIPCI %p %s using driver \"%s\"\n", device,
	       efi_devpath_text ( efipci->path ), efipci->pci.id->name );

	return 0;

	pci_remove ( &efipci->pci );
 err_probe:
 err_enable:
 err_find_driver:
	efipci_destroy ( efipci );
 err_create:
 err_already_started:
	return rc;
}

/**
 * Detach driver from device
 *
 * @v device		EFI device
  */
static void efipci_stop ( EFI_HANDLE device ) {
	struct efi_pci_device *efipci;

	/* Find PCI device */
	efipci = efipci_find_efi ( device );
	if ( ! efipci )
		return;

	/* Remove device */
	pci_remove ( &efipci->pci );

	/* Delete EFI PCI device */
	efipci_destroy ( efipci );
}

/** EFI PCI driver */
struct efi_driver efipci_driver __efi_driver ( EFI_DRIVER_NORMAL ) = {
	.name = "PCI",
	.supported = efipci_supported,
	.start = efipci_start,
	.stop = efipci_stop,
};
