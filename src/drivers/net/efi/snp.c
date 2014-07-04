/*
 * Copyright (C) 2014 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/SimpleNetwork.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/efi_snp.h>
#include <ipxe/efi/efi_pci.h>
#include "snpnet.h"
#include "snp.h"

/** @file
 *
 * SNP driver
 *
 */

/** EFI simple network protocol GUID */
static EFI_GUID efi_simple_network_protocol_guid
	= EFI_SIMPLE_NETWORK_PROTOCOL_GUID;

/** EFI PCI I/O protocol GUID */
static EFI_GUID efi_pci_io_protocol_guid
	= EFI_PCI_IO_PROTOCOL_GUID;

/**
 * Check to see if driver supports a device
 *
 * @v device		EFI device handle
 * @ret rc		Return status code
 */
static int snp_supported ( EFI_HANDLE device ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_STATUS efirc;

	/* Check that this is not a device we are providing ourselves */
	if ( find_snpdev ( device ) != NULL ) {
		DBGCP ( device, "SNP %p %s is provided by this binary\n",
			device, efi_handle_devpath_text ( device ) );
		return -ENOTTY;
	}

	/* Test for presence of simple network protocol */
	if ( ( efirc = bs->OpenProtocol ( device,
					  &efi_simple_network_protocol_guid,
					  NULL, efi_image_handle, device,
					  EFI_OPEN_PROTOCOL_TEST_PROTOCOL))!=0){
		DBGCP ( device, "SNP %p %s is not an SNP device\n",
			device, efi_handle_devpath_text ( device ) );
		return -EEFI ( efirc );
	}
	DBGC ( device, "SNP %p %s is an SNP device\n",
	       device, efi_handle_devpath_text ( device ) );

	return 0;
}

/**
 * Get underlying PCI device information
 *
 * @v snpdev		SNP device
 * @ret rc		Return status code
 */
static int snp_pci_info ( struct snp_device *snpdev ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct efi_device *efidev = snpdev->efidev;
	EFI_DEVICE_PATH_PROTOCOL *devpath = efidev->path;
	struct pci_device pci;
	EFI_HANDLE device;
	EFI_STATUS efirc;
	int rc;

	/* Check for presence of PCI I/O protocol */
	if ( ( efirc = bs->LocateDevicePath ( &efi_pci_io_protocol_guid,
					      &devpath, &device ) ) != 0 ) {
		DBGC ( efidev->device, "SNP %p %s is not a PCI device\n",
		       efidev->device, efi_devpath_text ( efidev->path ) );
		return -EEFI ( efirc );
	}

	/* Get PCI device information */
	if ( ( rc = efipci_info ( device, &pci ) ) != 0 ) {
		DBGC ( efidev->device, "SNP %p %s could not get PCI "
		       "information: %s\n", efidev->device,
		       efi_devpath_text ( efidev->path ), strerror ( rc ) );
		return rc;
	}

	/* Populate SNP device information */
	memcpy ( &snpdev->dev.desc, &pci.dev.desc, sizeof ( snpdev->dev.desc ));
	snprintf ( snpdev->dev.name, sizeof ( snpdev->dev.name ), "SNP-%s",
		   pci.dev.name );

	return 0;
}

/**
 * Attach driver to device
 *
 * @v efidev		EFI device
 * @ret rc		Return status code
 */
static int snp_start ( struct efi_device *efidev ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE device = efidev->device;
	struct snp_device *snpdev;
	union {
		EFI_SIMPLE_NETWORK_PROTOCOL *snp;
		void *interface;
	} snp;
	EFI_STATUS efirc;
	int rc;

	/* Check that this is not a device we are providing ourselves */
	if ( find_snpdev ( efidev->device ) != NULL ) {
		DBGCP ( device, "SNP %p %s is provided by this binary\n",
			device, efi_devpath_text ( efidev->path ) );
		rc = -ENOTTY;
		goto err_own;
	}

	/* Allocate and initialise structure */
	snpdev = zalloc ( sizeof ( *snpdev ) );
	if ( ! snpdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	snpdev->efidev = efidev;
	snpdev->dev.driver_name = "SNP";
	INIT_LIST_HEAD ( &snpdev->dev.children );

	/* See if device is an SNP device */
	if ( ( efirc = bs->OpenProtocol ( device,
					  &efi_simple_network_protocol_guid,
					  &snp.interface, efi_image_handle,
					  device,
					  ( EFI_OPEN_PROTOCOL_BY_DRIVER |
					    EFI_OPEN_PROTOCOL_EXCLUSIVE )))!=0){
		rc = -EEFI ( efirc );
		DBGCP ( device, "SNP %p %s cannot open SNP protocol: %s\n",
			device, efi_devpath_text ( efidev->path ),
			strerror ( rc ) );
		goto err_open_protocol;
	}
	snpdev->snp = snp.snp;

	/* Get underlying device information */
	if ( ( rc = snp_pci_info ( snpdev ) ) != 0 )
		goto err_info;

	/* Mark SNP device as a child of the EFI device */
	snpdev->dev.parent = &efidev->dev;
	list_add ( &snpdev->dev.siblings, &efidev->dev.children );

	/* Create SNP network device */
	if ( ( rc = snpnet_probe ( snpdev ) ) != 0 )
		goto err_probe;

	efidev_set_drvdata ( efidev, snpdev );
	return 0;

	snpnet_remove ( snpdev );
 err_probe:
	list_del ( &snpdev->dev.siblings );
 err_info:
	bs->CloseProtocol ( device, &efi_simple_network_protocol_guid,
			    efi_image_handle, device );
 err_open_protocol:
	free ( snpdev );
 err_alloc:
 err_own:
	return rc;
}

/**
 * Detach driver from device
 *
 * @v efidev		EFI device
  */
static void snp_stop ( struct efi_device *efidev ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct snp_device *snpdev = efidev_get_drvdata ( efidev );

	snpnet_remove ( snpdev );
	list_del ( &snpdev->dev.siblings );
	bs->CloseProtocol ( efidev->device, &efi_simple_network_protocol_guid,
			    efi_image_handle, efidev->device );
	free ( snpdev );
}

/** EFI SNP driver */
struct efi_driver snp_driver __efi_driver ( EFI_DRIVER_NORMAL ) = {
	.name = "SNP",
	.supported = snp_supported,
	.start = snp_start,
	.stop = snp_stop,
};
