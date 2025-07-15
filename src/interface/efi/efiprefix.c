/*
 * Copyright (C) 2009 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <ipxe/device.h>
#include <ipxe/uri.h>
#include <ipxe/init.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/efi_snp.h>
#include <ipxe/efi/efi_autoboot.h>
#include <ipxe/efi/efi_autoexec.h>
#include <ipxe/efi/efi_cachedhcp.h>
#include <ipxe/efi/efi_watchdog.h>
#include <ipxe/efi/efi_path.h>
#include <ipxe/efi/efi_veto.h>

/**
 * EFI entry point
 *
 * @v image_handle	Image handle
 * @v systab		System table
 * @ret efirc		EFI return status code
 */
EFI_STATUS EFIAPI _efi_start ( EFI_HANDLE image_handle,
			       EFI_SYSTEM_TABLE *systab ) {
	EFI_STATUS efirc;
	int rc;

	/* Initialise stack cookie */
	efi_init_stack_guard ( image_handle );

	/* Initialise EFI environment */
	if ( ( efirc = efi_init ( image_handle, systab ) ) != 0 )
		goto err_init;

	/* Claim SNP devices for use by iPXE */
	efi_snp_claim();

	/* Start watchdog holdoff timer */
	efi_watchdog_start();

	/* Call to main() */
	if ( ( rc = main() ) != 0 ) {
		efirc = EFIRC ( rc );
		goto err_main;
	}

 err_main:
	efi_watchdog_stop();
	efi_snp_release();
	efi_loaded_image->Unload ( image_handle );
	efi_driver_reconnect_all();
 err_init:
	return efirc;
}

/**
 * Initialise EFI application
 *
 */
static void efi_init_application ( void ) {
	EFI_HANDLE device = efi_loaded_image->DeviceHandle;
	EFI_DEVICE_PATH_PROTOCOL *devpath = efi_loaded_image_path;
	struct uri *uri;

	/* Set current working URI from device path, if present */
	uri = efi_path_uri ( devpath );
	if ( uri )
		churi ( uri );
	uri_put ( uri );

	/* Identify autoboot device, if any */
	efi_set_autoboot_ll_addr ( device, devpath );

	/* Store cached DHCP packet, if any */
	efi_cachedhcp_record ( device, devpath );
}

/** EFI application initialisation function */
struct init_fn efi_init_application_fn __init_fn ( INIT_NORMAL ) = {
	.name = "efi",
	.initialise = efi_init_application,
};

/**
 * Probe EFI root bus
 *
 * @v rootdev		EFI root device
 */
static int efi_probe ( struct root_device *rootdev __unused ) {

	/* Try loading autoexec script */
	efi_autoexec_load();

	/* Remove any vetoed drivers */
	efi_veto();

	/* Connect our drivers */
	return efi_driver_connect_all();
}

/**
 * Remove EFI root bus
 *
 * @v rootdev		EFI root device
 */
static void efi_remove ( struct root_device *rootdev __unused ) {

	/* Disconnect our drivers */
	efi_driver_disconnect_all();
}

/** EFI root device driver */
static struct root_driver efi_root_driver = {
	.probe = efi_probe,
	.remove = efi_remove,
};

/** EFI root device */
struct root_device efi_root_device __root_device = {
	.dev = { .name = "EFI" },
	.driver = &efi_root_driver,
};
