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
#include <ipxe/image.h>
#include <ipxe/init.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_autoboot.h>
#include <ipxe/efi/Protocol/SimpleNetwork.h>
#include <ipxe/efi/Protocol/SimpleFileSystem.h>
#include <ipxe/efi/Guid/FileInfo.h>
#include <usr/autoboot.h>

/** @file
 *
 * EFI automatic booting
 *
 */

/** Autoexec script filename */
#define AUTOEXEC_FILENAME L"autoexec.ipxe"

/** Autoexec script image name */
#define AUTOEXEC_NAME "autoexec.ipxe"

/** Autoexec script (if any) */
static void *efi_autoexec;

/** Autoexec script length */
static size_t efi_autoexec_len;

/**
 * Identify autoboot device
 *
 * @v device		Device handle
 * @ret rc		Return status code
 */
static int efi_set_autoboot_ll_addr ( EFI_HANDLE device ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	union {
		EFI_SIMPLE_NETWORK_PROTOCOL *snp;
		void *interface;
	} snp;
	EFI_SIMPLE_NETWORK_MODE *mode;
	EFI_STATUS efirc;
	int rc;

	/* Look for an SNP instance on the image's device handle */
	if ( ( efirc = bs->OpenProtocol ( device,
					  &efi_simple_network_protocol_guid,
					  &snp.interface, efi_image_handle,
					  NULL,
					  EFI_OPEN_PROTOCOL_GET_PROTOCOL ))!=0){
		rc = -EEFI ( efirc );
		DBGC ( device, "EFI %s has no SNP instance: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		return rc;
	}

	/* Record autoboot device */
	mode = snp.snp->Mode;
	set_autoboot_ll_addr ( &mode->CurrentAddress, mode->HwAddressSize );
	DBGC ( device, "EFI %s found autoboot link-layer address:\n",
	       efi_handle_name ( device ) );
	DBGC_HDA ( device, 0, &mode->CurrentAddress, mode->HwAddressSize );

	/* Close protocol */
	bs->CloseProtocol ( device, &efi_simple_network_protocol_guid,
			    efi_image_handle, NULL );

	return 0;
}

/**
 * Load autoexec script
 *
 * @v device		Device handle
 * @ret rc		Return status code
 */
static int efi_load_autoexec ( EFI_HANDLE device ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	static wchar_t name[] = AUTOEXEC_FILENAME;
	union {
		void *interface;
		EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
	} u;
	struct {
		EFI_FILE_INFO info;
		CHAR16 name[ sizeof ( name ) / sizeof ( name[0] ) ];
	} info;
	EFI_FILE_PROTOCOL *root;
	EFI_FILE_PROTOCOL *file;
	UINTN size;
	VOID *data;
	EFI_STATUS efirc;
	int rc;

	/* Sanity check */
	assert ( efi_autoexec == UNULL );
	assert ( efi_autoexec_len == 0 );

	/* Open simple file system protocol */
	if ( ( efirc = bs->OpenProtocol ( device,
					  &efi_simple_file_system_protocol_guid,
					  &u.interface, efi_image_handle,
					  device,
					  EFI_OPEN_PROTOCOL_GET_PROTOCOL ))!=0){
		rc = -EEFI ( efirc );
		DBGC ( device, "EFI %s has no filesystem instance: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		goto err_filesystem;
	}

	/* Open root directory */
	if ( ( efirc = u.fs->OpenVolume ( u.fs, &root ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( device, "EFI %s could not open volume: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		goto err_volume;
	}

	/* Open autoexec script */
	if ( ( efirc = root->Open ( root, &file, name,
				    EFI_FILE_MODE_READ, 0 ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( device, "EFI %s has no %ls: %s\n",
		       efi_handle_name ( device ), name, strerror ( rc ) );
		goto err_open;
	}

	/* Get file information */
	size = sizeof ( info );
	if ( ( efirc = file->GetInfo ( file, &efi_file_info_id, &size,
				       &info ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( device, "EFI %s could not get %ls info: %s\n",
		       efi_handle_name ( device ), name, strerror ( rc ) );
		goto err_getinfo;
	}
	size = info.info.FileSize;

	/* Ignore zero-length files */
	if ( ! size ) {
		rc = -EINVAL;
		DBGC ( device, "EFI %s has zero-length %ls\n",
		       efi_handle_name ( device ), name );
		goto err_empty;
	}

	/* Allocate temporary copy */
	if ( ( efirc = bs->AllocatePool ( EfiBootServicesData, size,
					  &data ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( device, "EFI %s could not allocate %ls: %s\n",
		       efi_handle_name ( device ), name, strerror ( rc ) );
		goto err_alloc;
	}

	/* Read file */
	if ( ( efirc = file->Read ( file, &size, data ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( device, "EFI %s could not read %ls: %s\n",
		       efi_handle_name ( device ), name, strerror ( rc ) );
		goto err_read;
	}

	/* Record autoexec script */
	efi_autoexec = data;
	efi_autoexec_len = size;
	data = NULL;
	DBGC ( device, "EFI %s found %ls\n",
	       efi_handle_name ( device ), name );

	/* Success */
	rc = 0;

 err_read:
	if ( data )
		bs->FreePool ( data );
 err_alloc:
 err_empty:
 err_getinfo:
	file->Close ( file );
 err_open:
	root->Close ( root );
 err_volume:
	bs->CloseProtocol ( device, &efi_simple_file_system_protocol_guid,
			    efi_image_handle, device );
 err_filesystem:
	return rc;
}

/**
 * Configure automatic booting
 *
 */
void efi_set_autoboot ( void ) {
	EFI_HANDLE device = efi_loaded_image->DeviceHandle;

	/* Identify autoboot device, if any */
	efi_set_autoboot_ll_addr ( device );

	/* Load autoexec script, if any */
	efi_load_autoexec ( device );
}

/**
 * Register automatic boot image
 *
 */
static void efi_autoboot_startup ( void ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE device = efi_loaded_image->DeviceHandle;
	const char *name = AUTOEXEC_NAME;
	struct image *image;

	/* Do nothing if we have no autoexec script */
	if ( ! efi_autoexec )
		return;

	/* Create autoexec image */
	image = image_memory ( name, virt_to_user ( efi_autoexec ),
			       efi_autoexec_len );
	if ( ! image ) {
		DBGC ( device, "EFI %s could not create %s\n",
		       efi_handle_name ( device ), name );
		return;
	}
	DBGC ( device, "EFI %s registered %s\n",
	       efi_handle_name ( device ), name );

	/* Free temporary copy */
	bs->FreePool ( efi_autoexec );
	efi_autoexec = NULL;
}

/** Automatic boot startup function */
struct startup_fn efi_autoboot_startup_fn __startup_fn ( STARTUP_NORMAL ) = {
	.name = "efi_autoboot",
	.startup = efi_autoboot_startup,
};
