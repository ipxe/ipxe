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

#include <string.h>
#include <errno.h>
#include <ipxe/init.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/efi_utils.h>
#include <ipxe/efi/mnpnet.h>
#include <ipxe/efi/Protocol/SimpleNetwork.h>
#include <ipxe/efi/Protocol/NetworkInterfaceIdentifier.h>
#include "snpnet.h"
#include "nii.h"

/** @file
 *
 * EFI chainloaded-device-only driver
 *
 */

/** A chainloaded protocol */
struct chained_protocol {
	/** Protocol GUID */
	EFI_GUID *protocol;
	/**
	 * Target device handle
	 *
	 * This is the uppermost handle on which the same protocol
	 * instance is installed as we find on the loaded image's
	 * device handle.
	 *
	 * We match against the protocol instance (rather than simply
	 * matching against the device handle itself) because some
	 * systems load us via a child of the underlying device, with
	 * a duplicate protocol installed on the child handle.
	 *
	 * We record the handle rather than the protocol instance
	 * pointer since the calls to DisconnectController() and
	 * ConnectController() may end up uninstalling and
	 * reinstalling the protocol instance.
	 */
	EFI_HANDLE device;
};

/** Chainloaded SNP protocol */
static struct chained_protocol chained_snp = {
	.protocol = &efi_simple_network_protocol_guid,
};

/** Chainloaded NII protocol */
static struct chained_protocol chained_nii = {
	.protocol = &efi_nii31_protocol_guid,
};

/** Chainloaded MNP protocol */
static struct chained_protocol chained_mnp = {
	.protocol = &efi_managed_network_service_binding_protocol_guid,
};

/**
 * Locate chainloaded protocol
 *
 * @v chained		Chainloaded protocol
 */
static void chained_locate ( struct chained_protocol *chained ) {
	EFI_HANDLE device = efi_loaded_image->DeviceHandle;
	EFI_HANDLE handle;
	void *match = NULL;
	void *interface;
	unsigned int skip;
	int rc;

	/* Identify target device handle */
	for ( skip = 0 ; ; skip++ ) {

		/* Locate handle supporting this protocol */
		if ( ( rc = efi_locate_device ( device, chained->protocol,
						&handle, skip ) ) != 0 ) {
			if ( skip == 0 ) {
				DBGC ( device, "CHAINED %s does not support "
				       "%s: %s\n", efi_handle_name ( device ),
				       efi_guid_ntoa ( chained->protocol ),
				       strerror ( rc ) );
			}
			break;
		}

		/* Get protocol instance */
		if ( ( rc = efi_open ( handle, chained->protocol,
				       &interface ) ) != 0 ) {
			DBGC ( device, "CHAINED %s could not open %s on ",
			       efi_handle_name ( device ),
			       efi_guid_ntoa ( chained->protocol ) );
			DBGC ( device, "%s: %s\n",
			       efi_handle_name ( handle ), strerror ( rc ) );
			break;
		}

		/* Stop if we reach a non-matching protocol instance */
		if ( match && ( match != interface ) ) {
			DBGC ( device, "CHAINED %s found non-matching %s on ",
			       efi_handle_name ( device ),
			       efi_guid_ntoa ( chained->protocol ) );
			DBGC ( device, "%s\n", efi_handle_name ( handle ) );
			break;
		}

		/* Record this handle */
		chained->device = handle;
		match = interface;
		DBGC ( device, "CHAINED %s found %s on ",
		       efi_handle_name ( device ),
		       efi_guid_ntoa ( chained->protocol ) );
		DBGC ( device, "%s\n", efi_handle_name ( chained->device ) );
	}
}

/**
 * Check to see if driver supports a device
 *
 * @v device		EFI device handle
 * @v chained		Chainloaded protocol
 * @ret rc		Return status code
 */
static int chained_supported ( EFI_HANDLE device,
			       struct chained_protocol *chained ) {
	void *interface;
	int rc;

	/* Get protocol */
	if ( ( rc = efi_open ( device, chained->protocol,
			       &interface ) ) != 0 ) {
		DBGCP ( device, "CHAINED %s is not a %s device\n",
			efi_handle_name ( device ),
			efi_guid_ntoa ( chained->protocol ) );
		return rc;
	}

	/* Ignore non-matching handles */
	if ( device != chained->device ) {
		DBGC2 ( device, "CHAINED %s is not the chainloaded %s\n",
			efi_handle_name ( device ),
			efi_guid_ntoa ( chained->protocol ) );
		return -ENOTTY;
	}

	DBGC ( device, "CHAINED %s is the chainloaded %s\n",
	       efi_handle_name ( device ),
	       efi_guid_ntoa ( chained->protocol ) );
	return 0;
}

/**
 * Check to see if driver supports a device
 *
 * @v device		EFI device handle
 * @ret rc		Return status code
 */
static int snponly_supported ( EFI_HANDLE device ) {

	return chained_supported ( device, &chained_snp );
}

/**
 * Check to see if driver supports a device
 *
 * @v device		EFI device handle
 * @ret rc		Return status code
 */
static int niionly_supported ( EFI_HANDLE device ) {

	return chained_supported ( device, &chained_nii );
}

/**
 * Check to see if driver supports a device
 *
 * @v device		EFI device handle
 * @ret rc		Return status code
 */
static int mnponly_supported ( EFI_HANDLE device ) {

	return chained_supported ( device, &chained_mnp );
}

/** EFI SNP chainloading-device-only driver */
struct efi_driver snponly_driver __efi_driver ( EFI_DRIVER_SNP ) = {
	.name = "SNPONLY",
	.supported = snponly_supported,
	.exclude = snpnet_exclude,
	.start = snpnet_start,
	.stop = snpnet_stop,
};

/** EFI NII chainloading-device-only driver */
struct efi_driver niionly_driver __efi_driver ( EFI_DRIVER_NII ) = {
	.name = "NIIONLY",
	.supported = niionly_supported,
	.exclude = nii_exclude,
	.start = nii_start,
	.stop = nii_stop,
};

/** EFI MNP chainloading-device-only driver */
struct efi_driver mnponly_driver __efi_driver ( EFI_DRIVER_MNP ) = {
	.name = "MNPONLY",
	.supported = mnponly_supported,
	.start = mnpnet_start,
	.stop = mnpnet_stop,
};

/**
 * Initialise EFI chainloaded-device-only driver
 *
 */
static void chained_init ( void ) {

	chained_locate ( &chained_snp );
	chained_locate ( &chained_nii );
	chained_locate ( &chained_mnp );
}

/** EFI chainloaded-device-only initialisation function */
struct init_fn chained_init_fn __init_fn ( INIT_LATE ) = {
	.initialise = chained_init,
};
