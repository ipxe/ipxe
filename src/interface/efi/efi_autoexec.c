/*
 * Copyright (C) 2021 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <ipxe/efi/efi_autoexec.h>
#include <ipxe/efi/Protocol/SimpleFileSystem.h>
#include <ipxe/efi/Guid/FileInfo.h>

/** @file
 *
 * EFI autoexec script
 *
 */

/** Autoexec script filename */
static wchar_t efi_autoexec_wname[] = L"autoexec.ipxe";

/** Autoexec script image name */
static char efi_autoexec_name[] = "autoexec.ipxe";

/** Autoexec script (if any) */
static void *efi_autoexec;

/** Autoexec script length */
static size_t efi_autoexec_len;

/**
 * Load autoexec script from filesystem
 *
 * @v device		Device handle
 * @ret rc		Return status code
 */
static int efi_autoexec_filesystem ( EFI_HANDLE device ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	union {
		void *interface;
		EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
	} u;
	struct {
		EFI_FILE_INFO info;
		CHAR16 name[ sizeof ( efi_autoexec_wname ) /
			     sizeof ( efi_autoexec_wname[0] ) ];
	} info;
	EFI_FILE_PROTOCOL *root;
	EFI_FILE_PROTOCOL *file;
	UINTN size;
	VOID *data;
	EFI_STATUS efirc;
	int rc;

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
	if ( ( efirc = root->Open ( root, &file, efi_autoexec_wname,
				    EFI_FILE_MODE_READ, 0 ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( device, "EFI %s has no %ls: %s\n",
		       efi_handle_name ( device ), efi_autoexec_wname,
		       strerror ( rc ) );
		goto err_open;
	}

	/* Get file information */
	size = sizeof ( info );
	if ( ( efirc = file->GetInfo ( file, &efi_file_info_id, &size,
				       &info ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( device, "EFI %s could not get %ls info: %s\n",
		       efi_handle_name ( device ), efi_autoexec_wname,
		       strerror ( rc ) );
		goto err_getinfo;
	}
	size = info.info.FileSize;

	/* Ignore zero-length files */
	if ( ! size ) {
		rc = -EINVAL;
		DBGC ( device, "EFI %s has zero-length %ls\n",
		       efi_handle_name ( device ), efi_autoexec_wname );
		goto err_empty;
	}

	/* Allocate temporary copy */
	if ( ( efirc = bs->AllocatePool ( EfiBootServicesData, size,
					  &data ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( device, "EFI %s could not allocate %ls: %s\n",
		       efi_handle_name ( device ), efi_autoexec_wname,
		       strerror ( rc ) );
		goto err_alloc;
	}

	/* Read file */
	if ( ( efirc = file->Read ( file, &size, data ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( device, "EFI %s could not read %ls: %s\n",
		       efi_handle_name ( device ), efi_autoexec_wname,
		       strerror ( rc ) );
		goto err_read;
	}

	/* Record autoexec script */
	efi_autoexec = data;
	efi_autoexec_len = size;
	data = NULL;
	DBGC ( device, "EFI %s found %ls\n",
	       efi_handle_name ( device ), efi_autoexec_wname );

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
 * Load autoexec script
 *
 * @v device		Device handle
 * @ret rc		Return status code
 */
int efi_autoexec_load ( EFI_HANDLE device ) {
	int rc;

	/* Sanity check */
	assert ( efi_autoexec == NULL );
	assert ( efi_autoexec_len == 0 );

	/* Try loading from file system, if supported */
	if ( ( rc = efi_autoexec_filesystem ( device ) ) == 0 )
		return 0;

	return -ENOENT;
}

/**
 * Register autoexec script
 *
 */
static void efi_autoexec_startup ( void ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE device = efi_loaded_image->DeviceHandle;
	struct image *image;

	/* Do nothing if we have no autoexec script */
	if ( ! efi_autoexec )
		return;

	/* Create autoexec image */
	image = image_memory ( efi_autoexec_name,
			       virt_to_user ( efi_autoexec ),
			       efi_autoexec_len );
	if ( ! image ) {
		DBGC ( device, "EFI %s could not create %s\n",
		       efi_handle_name ( device ), efi_autoexec_name );
		return;
	}
	DBGC ( device, "EFI %s registered %s\n",
	       efi_handle_name ( device ), efi_autoexec_name );

	/* Free temporary copy */
	bs->FreePool ( efi_autoexec );
	efi_autoexec = NULL;
}

/** Autoexec script startup function */
struct startup_fn efi_autoexec_startup_fn __startup_fn ( STARTUP_NORMAL ) = {
	.name = "efi_autoexec",
	.startup = efi_autoexec_startup,
};
