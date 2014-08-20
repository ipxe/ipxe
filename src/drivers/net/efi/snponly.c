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

#include <errno.h>
#include <ipxe/init.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/Protocol/SimpleNetwork.h>
#include "snpnet.h"

/** @file
 *
 * SNP chainloading-device-only driver
 *
 */

/**
 * SNP protocol instance installed on the loaded image's device handle
 *
 * We match against the SNP protocol instance (rather than simply
 * matching against the device handle itself) because some systems
 * load us via a child of the SNP device, with a duplicate SNP
 * protocol installed on the child handle.
 */
static EFI_SIMPLE_NETWORK_PROTOCOL *snponly;

/**
 * Check to see if driver supports a device
 *
 * @v device		EFI device handle
 * @ret rc		Return status code
 */
static int snponly_supported ( EFI_HANDLE device ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_STATUS efirc;
	union {
		EFI_SIMPLE_NETWORK_PROTOCOL *snp;
		void *interface;
	} snp;
	int rc;

	/* Get SNP protocol */
	if ( ( efirc = bs->OpenProtocol ( device,
					  &efi_simple_network_protocol_guid,
					  &snp.interface, efi_image_handle,
					  device,
					  EFI_OPEN_PROTOCOL_GET_PROTOCOL ))!=0){
		rc = -EEFI ( efirc );
		DBGCP ( device, "SNPONLY %p %s is not an SNP device\n",
			device, efi_handle_name ( device ) );
		goto err_not_snp;
	}

	/* Test for a match against the chainloading device */
	if ( snp.snp != snponly ) {
		DBGC ( device, "SNPONLY %p %s SNP %p is not the "
		       "chainloading SNP\n", device,
		       efi_handle_name ( device ), snp.snp );
		rc = -ENOTTY;
		goto err_not_snponly;
	}

	/* Success */
	rc = 0;
	DBGC ( device, "SNPONLY %p %s SNP %p is the chainloading SNP\n",
	       device, efi_handle_name ( device ), snp.snp );

 err_not_snponly:
	bs->CloseProtocol ( device, &efi_simple_network_protocol_guid,
			    efi_image_handle, device );
 err_not_snp:
	return rc;
}

/** EFI chainloading-device-only driver */
struct efi_driver snponly_driver __efi_driver ( EFI_DRIVER_NORMAL ) = {
	.name = "SNPONLY",
	.supported = snponly_supported,
	.start = snpnet_start,
	.stop = snpnet_stop,
};

/**
 * Initialise EFI chainloading-device-only driver
 *
 */
static void snponly_init ( void ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE device = efi_loaded_image->DeviceHandle;
	union {
		EFI_SIMPLE_NETWORK_PROTOCOL *snp;
		void *interface;
	} snp;
	EFI_STATUS efirc;

	/* Look for SNP protocol on the loaded image's device handle */
	if ( ( efirc = bs->OpenProtocol ( device,
					  &efi_simple_network_protocol_guid,
					  &snp.interface, efi_image_handle,
					  device,
					  EFI_OPEN_PROTOCOL_GET_PROTOCOL ))!=0){
		DBGC ( device, "SNPONLY %p %s is not an SNP device\n",
		       device, efi_handle_name ( device ) );
		goto err_open_protocol;
	}

	/* Record SNP protocol instance */
	snponly = snp.snp;
	DBGC ( device, "SNPONLY %p %s found chainloading SNP %p\n",
	       device, efi_handle_name ( device ), snponly );

 err_open_protocol:
	bs->CloseProtocol ( device, &efi_simple_network_protocol_guid,
			    efi_image_handle, device );
}

/** EFI chainloading-device-only initialisation function */
struct init_fn snponly_init_fn __init_fn ( INIT_LATE ) = {
	.initialise = snponly_init,
};
