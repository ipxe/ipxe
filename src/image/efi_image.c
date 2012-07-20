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

#include <errno.h>
#include <stdlib.h>
#include <ipxe/efi/efi.h>
#include <ipxe/image.h>
#include <ipxe/init.h>
#include <ipxe/features.h>
#include <ipxe/uri.h>

FEATURE ( FEATURE_IMAGE, "EFI", DHCP_EB_FEATURE_EFI, 1 );

/** EFI loaded image protocol GUID */
static EFI_GUID efi_loaded_image_protocol_guid =
	EFI_LOADED_IMAGE_PROTOCOL_GUID;

/**
 * Create a Unicode command line for the image
 *
 * @v image             EFI image
 * @v devpath_out       Device path to pass to image (output)
 * @v cmdline_out       Unicode command line (output)
 * @v cmdline_len_out   Length of command line in bytes (output)
 * @ret rc              Return status code
 */
static int efi_image_make_cmdline ( struct image *image,
				    EFI_DEVICE_PATH **devpath_out,
				    VOID **cmdline_out,
				    UINT32 *cmdline_len_out ) {
	char *uri;
	size_t uri_len;
	FILEPATH_DEVICE_PATH *devpath;
	EFI_DEVICE_PATH *endpath;
	size_t devpath_len;
	CHAR16 *cmdline;
	UINT32 cmdline_len;
	size_t args_len = 0;
	UINT32 i;

	/* Get the URI string of the image */
	uri_len = unparse_uri ( NULL, 0, image->uri, URI_ALL ) + 1;

	/* Compute final command line length */
	if ( image->cmdline ) {
		args_len = strlen ( image->cmdline ) + 1;
	}
	cmdline_len = args_len + uri_len;

	/* Allocate space for the uri, final command line and device path */
	cmdline = malloc ( cmdline_len * sizeof ( CHAR16 ) + uri_len
			   + SIZE_OF_FILEPATH_DEVICE_PATH
			   + uri_len * sizeof ( CHAR16 )
			   + sizeof ( EFI_DEVICE_PATH ) );
	if ( ! cmdline )
		return -ENOMEM;
	uri = (char *) ( cmdline + cmdline_len );
	devpath = (FILEPATH_DEVICE_PATH *) ( uri + uri_len );
	endpath = (EFI_DEVICE_PATH *) ( (char *) devpath
					+ SIZE_OF_FILEPATH_DEVICE_PATH
					+ uri_len * sizeof ( CHAR16 ) );

	/* Build the iPXE device path */
	devpath->Header.Type = MEDIA_DEVICE_PATH;
	devpath->Header.SubType = MEDIA_FILEPATH_DP;
	devpath_len = SIZE_OF_FILEPATH_DEVICE_PATH
			+ uri_len * sizeof ( CHAR16 );
	devpath->Header.Length[0] = devpath_len & 0xFF;
	devpath->Header.Length[1] = devpath_len >> 8;
	endpath->Type = END_DEVICE_PATH_TYPE;
	endpath->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
	endpath->Length[0] = 4;
	endpath->Length[1] = 0;
	unparse_uri ( uri, uri_len, image->uri, URI_ALL );

	/* Convert to Unicode */
	for ( i = 0 ; i < uri_len ; i++ ) {
		cmdline[i] = uri[i];
		devpath->PathName[i] = uri[i];
	}
	if ( image->cmdline ) {
		cmdline[uri_len - 1] = ' ';
	}
	for ( i = 0 ; i < args_len ; i++ ) {
		cmdline[i + uri_len] = image->cmdline[i];
	}

	*devpath_out = &devpath->Header;
	*cmdline_out = cmdline;
	*cmdline_len_out = cmdline_len * sizeof ( CHAR16 );
	return 0;
}

/**
 * Execute EFI image
 *
 * @v image		EFI image
 * @ret rc		Return status code
 */
static int efi_image_exec ( struct image *image ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	union {
		EFI_LOADED_IMAGE_PROTOCOL *image;
		void *interface;
	} loaded;
	EFI_HANDLE handle;
	EFI_HANDLE device_handle = NULL;
	UINTN exit_data_size;
	CHAR16 *exit_data;
	EFI_STATUS efirc;
	int rc;

	/* Attempt loading image */
	if ( ( efirc = bs->LoadImage ( FALSE, efi_image_handle, NULL,
				       user_to_virt ( image->data, 0 ),
				       image->len, &handle ) ) != 0 ) {
		/* Not an EFI image */
		DBGC ( image, "EFIIMAGE %p could not load: %s\n",
		       image, efi_strerror ( efirc ) );
		rc = -ENOEXEC;
		goto err_load_image;
	}

	/* Get the loaded image protocol for the newly loaded image */
	efirc = bs->OpenProtocol ( handle, &efi_loaded_image_protocol_guid,
				   &loaded.interface, efi_image_handle,
				   NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL );
	if ( efirc ) {
		/* Should never happen */
		rc = EFIRC_TO_RC ( efirc );
		goto err_open_protocol;
	}

	/* Pass an iPXE download protocol to the image */
	if ( ( rc = efi_download_install ( &device_handle ) ) != 0 ) {
		DBGC ( image, "EFIIMAGE %p could not install iPXE download "
		       "protocol: %s\n", image, strerror ( rc ) );
		goto err_download_install;
	}
	loaded.image->DeviceHandle = device_handle;
	loaded.image->ParentHandle = efi_loaded_image;
	if ( ( rc = efi_image_make_cmdline ( image, &loaded.image->FilePath,
				       &loaded.image->LoadOptions,
				       &loaded.image->LoadOptionsSize ) ) != 0 )
		goto err_make_cmdline;

	/* Start the image */
	if ( ( efirc = bs->StartImage ( handle, &exit_data_size,
					&exit_data ) ) != 0 ) {
		DBGC ( image, "EFIIMAGE %p returned with status %s\n",
		       image, efi_strerror ( efirc ) );
		rc = EFIRC_TO_RC ( efirc );
		goto err_start_image;
	}

	/* Success */
	rc = 0;

 err_start_image:
	free ( loaded.image->LoadOptions );
 err_make_cmdline:
	efi_download_uninstall ( device_handle );
 err_download_install:
 err_open_protocol:
	/* Unload the image.  We can't leave it loaded, because we
	 * have no "unload" operation.
	 */
	bs->UnloadImage ( handle );
 err_load_image:

	return rc;
}

/**
 * Probe EFI image
 *
 * @v image		EFI file
 * @ret rc		Return status code
 */
static int efi_image_probe ( struct image *image ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE handle;
	EFI_STATUS efirc;

	/* Attempt loading image */
	if ( ( efirc = bs->LoadImage ( FALSE, efi_image_handle, NULL,
				       user_to_virt ( image->data, 0 ),
				       image->len, &handle ) ) != 0 ) {
		/* Not an EFI image */
		DBGC ( image, "EFIIMAGE %p could not load: %s\n",
		       image, efi_strerror ( efirc ) );
		return -ENOEXEC;
	}

	/* Unload the image.  We can't leave it loaded, because we
	 * have no "unload" operation.
	 */
	bs->UnloadImage ( handle );

	return 0;
}

/** EFI image type */
struct image_type efi_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "EFI",
	.probe = efi_image_probe,
	.exec = efi_image_exec,
};
