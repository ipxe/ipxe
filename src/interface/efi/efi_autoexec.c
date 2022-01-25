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
#include <stdlib.h>
#include <errno.h>
#include <ipxe/image.h>
#include <ipxe/init.h>
#include <ipxe/in.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_autoexec.h>
#include <ipxe/efi/Protocol/PxeBaseCode.h>
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
 * Load autoexec script from TFTP server
 *
 * @v device		Device handle
 * @ret rc		Return status code
 */
static int efi_autoexec_tftp ( EFI_HANDLE device ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	union {
		void *interface;
		EFI_PXE_BASE_CODE_PROTOCOL *pxe;
	} u;
	EFI_PXE_BASE_CODE_MODE *mode;
	EFI_PXE_BASE_CODE_PACKET *packet;
	union {
		struct in_addr in;
		EFI_IP_ADDRESS ip;
	} server;
	size_t filename_max;
	char *filename;
	char *sep;
	UINT64 size;
	VOID *data;
	EFI_STATUS efirc;
	int rc;

	/* Open PXE base code protocol */
	if ( ( efirc = bs->OpenProtocol ( device,
					  &efi_pxe_base_code_protocol_guid,
					  &u.interface, efi_image_handle,
					  device,
					  EFI_OPEN_PROTOCOL_GET_PROTOCOL ))!=0){
		rc = -EEFI ( efirc );
		DBGC ( device, "EFI %s has no PXE base code instance: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		goto err_pxe;
	}

	/* Do not attempt to parse DHCPv6 packets */
	mode = u.pxe->Mode;
	if ( mode->UsingIpv6 ) {
		rc = -ENOTSUP;
		DBGC ( device, "EFI %s has IPv6 PXE base code\n",
		       efi_handle_name ( device ) );
		goto err_ipv6;
	}

	/* Identify relevant reply packet */
	if ( mode->PxeReplyReceived &&
	     mode->PxeReply.Dhcpv4.BootpBootFile[0] ) {
		/* Use boot filename if present in PXE reply */
		DBGC ( device, "EFI %s using PXE reply filename\n",
		       efi_handle_name ( device ) );
		packet = &mode->PxeReply;
	} else if ( mode->DhcpAckReceived &&
		    mode->DhcpAck.Dhcpv4.BootpBootFile[0] ) {
		/* Otherwise, use boot filename if present in DHCPACK */
		DBGC ( device, "EFI %s using DHCPACK filename\n",
		       efi_handle_name ( device ) );
		packet = &mode->DhcpAck;
	} else if ( mode->ProxyOfferReceived &&
		    mode->ProxyOffer.Dhcpv4.BootpBootFile[0] ) {
		/* Otherwise, use boot filename if present in ProxyDHCPOFFER */
		DBGC ( device, "EFI %s using ProxyDHCPOFFER filename\n",
		       efi_handle_name ( device ) );
		packet = &mode->ProxyOffer;
	} else {
		/* No boot filename available */
		rc = -ENOENT;
		DBGC ( device, "EFI %s has no PXE boot filename\n",
		       efi_handle_name ( device ) );
		goto err_packet;
	}

	/* Allocate filename */
	filename_max = ( sizeof ( packet->Dhcpv4.BootpBootFile )
			 + ( sizeof ( efi_autoexec_name ) - 1 /* NUL */ )
			 + 1 /* NUL */ );
	filename = zalloc ( filename_max );
	if ( ! filename ) {
		rc = -ENOMEM;
		goto err_filename;
	}

	/* Extract next-server address and boot filename */
	memset ( &server, 0, sizeof ( server ) );
	memcpy ( &server.in, packet->Dhcpv4.BootpSiAddr,
		 sizeof ( server.in ) );
	memcpy ( filename, packet->Dhcpv4.BootpBootFile,
		 sizeof ( packet->Dhcpv4.BootpBootFile ) );

	/* Update filename to autoexec script name */
	sep = strrchr ( filename, '/' );
	if ( ! sep )
		sep = strrchr ( filename, '\\' );
	if ( ! sep )
		sep = ( filename - 1 );
	strcpy ( ( sep + 1 ), efi_autoexec_name );

	/* Get file size */
	if ( ( efirc = u.pxe->Mtftp ( u.pxe,
				      EFI_PXE_BASE_CODE_TFTP_GET_FILE_SIZE,
				      NULL, FALSE, &size, NULL, &server.ip,
				      ( ( UINT8 * ) filename ), NULL,
				      FALSE ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( device, "EFI %s could not get size of %s:%s: %s\n",
		       efi_handle_name ( device ), inet_ntoa ( server.in ),
		       filename, strerror ( rc ) );
		goto err_size;
	}

	/* Ignore zero-length files */
	if ( ! size ) {
		rc = -EINVAL;
		DBGC ( device, "EFI %s has zero-length %s:%s\n",
		       efi_handle_name ( device ), inet_ntoa ( server.in ),
		       filename );
		goto err_empty;
	}

	/* Allocate temporary copy */
	if ( ( efirc = bs->AllocatePool ( EfiBootServicesData, size,
					  &data ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( device, "EFI %s could not allocate %s:%s: %s\n",
		       efi_handle_name ( device ), inet_ntoa ( server.in ),
		       filename, strerror ( rc ) );
		goto err_alloc;
	}

	/* Download file */
	if ( ( efirc = u.pxe->Mtftp ( u.pxe, EFI_PXE_BASE_CODE_TFTP_READ_FILE,
				      data, FALSE, &size, NULL, &server.ip,
				      ( ( UINT8 * ) filename ), NULL,
				      FALSE ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( device, "EFI %s could not download %s:%s: %s\n",
		       efi_handle_name ( device ), inet_ntoa ( server.in ),
		       filename, strerror ( rc ) );
		goto err_download;
	}

	/* Record autoexec script */
	efi_autoexec = data;
	efi_autoexec_len = size;
	data = NULL;
	DBGC ( device, "EFI %s found %s:%s\n", efi_handle_name ( device ),
	       inet_ntoa ( server.in ), filename );

	/* Success */
	rc = 0;

 err_download:
	if ( data )
		bs->FreePool ( data );
 err_alloc:
 err_empty:
 err_size:
	free ( filename );
 err_filename:
 err_packet:
 err_ipv6:
	bs->CloseProtocol ( device, &efi_pxe_base_code_protocol_guid,
			    efi_image_handle, device );
 err_pxe:
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

	/* Try loading via TFTP, if supported */
	if ( ( rc = efi_autoexec_tftp ( device ) ) == 0 )
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
