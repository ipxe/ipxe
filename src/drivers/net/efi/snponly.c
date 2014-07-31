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
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_driver.h>
#include "snpnet.h"

/** @file
 *
 * SNP chainloaded-device-only driver
 *
 */

/**
 * Check to see if driver supports a device
 *
 * @v device		EFI device handle
 * @ret rc		Return status code
 */
static int snponly_supported ( EFI_HANDLE device ) {

	/* Check that this device is our loaded image's device */
	if ( device != efi_loaded_image->DeviceHandle )
		return -ENOTTY;

	DBGC ( device, "SNP %p %s is the SNP chainloading device\n",
	       device, efi_handle_name ( device ) );

	return 0;
}

/** EFI SNP driver */
struct efi_driver snponly_driver __efi_driver ( EFI_DRIVER_NORMAL ) = {
	.name = "SNPONLY",
	.supported = snponly_supported,
	.start = snpnet_start,
	.stop = snpnet_stop,
};
