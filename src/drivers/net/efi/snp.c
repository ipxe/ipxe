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
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <errno.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/efi_snp.h>
#include <ipxe/efi/efi_utils.h>
#include "snpnet.h"
#include "nii.h"

/** @file
 *
 * SNP driver
 *
 */

/**
 * Check to see if driver supports a device
 *
 * @v device		EFI device handle
 * @v protocol		Protocol GUID
 * @ret rc		Return status code
 */
static int snp_nii_supported ( EFI_HANDLE device, EFI_GUID *protocol ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE parent;
	EFI_STATUS efirc;
	int rc;

	/* Check that this is not a device we are providing ourselves */
	if ( find_snpdev ( device ) != NULL ) {
		DBGCP ( device, "HANDLE %s is provided by this binary\n",
			efi_handle_name ( device ) );
		return -ENOTTY;
	}

	/* Test for presence of protocol */
	if ( ( efirc = bs->OpenProtocol ( device, protocol,
					  NULL, efi_image_handle, device,
					  EFI_OPEN_PROTOCOL_TEST_PROTOCOL))!=0){
		DBGCP ( device, "HANDLE %s is not a %s device\n",
			efi_handle_name ( device ),
			efi_guid_ntoa ( protocol ) );
		return -EEFI ( efirc );
	}

	/* Check that there are no instances of this protocol further
	 * up this device path.
	 */
	if ( ( rc = efi_locate_device ( device, protocol,
					&parent, 1 ) ) == 0 ) {
		DBGC2 ( device, "HANDLE %s has %s-supporting parent ",
			efi_handle_name ( device ),
			efi_guid_ntoa ( protocol ) );
		DBGC2 ( device, "%s\n", efi_handle_name ( parent ) );
		return -ENOTTY;
	}

	DBGC ( device, "HANDLE %s is a %s device\n",
	       efi_handle_name ( device ), efi_guid_ntoa ( protocol ) );
	return 0;
}

/**
 * Check to see if driver supports a device
 *
 * @v device		EFI device handle
 * @ret rc		Return status code
 */
static int snp_supported ( EFI_HANDLE device ) {

	return snp_nii_supported ( device, &efi_simple_network_protocol_guid );
}

/**
 * Check to see if driver supports a device
 *
 * @v device		EFI device handle
 * @ret rc		Return status code
 */
static int nii_supported ( EFI_HANDLE device ) {

	return snp_nii_supported ( device, &efi_nii31_protocol_guid );
}

/** EFI SNP driver */
struct efi_driver snp_driver __efi_driver ( EFI_DRIVER_NORMAL ) = {
	.name = "SNP",
	.supported = snp_supported,
	.start = snpnet_start,
	.stop = snpnet_stop,
};

/** EFI NII driver */
struct efi_driver nii_driver __efi_driver ( EFI_DRIVER_NORMAL ) = {
	.name = "NII",
	.supported = nii_supported,
	.start = nii_start,
	.stop = nii_stop,
};
