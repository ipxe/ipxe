/*
 * Copyright (C) 2016 Michael Brown <mbrown@fensystems.co.uk>.
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

/**
 * @file
 *
 * EFI block device protocols
 *
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ipxe/refcnt.h>
#include <ipxe/list.h>
#include <ipxe/uri.h>
#include <ipxe/interface.h>
#include <ipxe/blockdev.h>
#include <ipxe/xfer.h>
#include <ipxe/open.h>
#include <ipxe/retry.h>
#include <ipxe/timer.h>
#include <ipxe/process.h>
#include <ipxe/sanboot.h>
#include <ipxe/iso9660.h>
#include <ipxe/acpi.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/BlockIo.h>
#include <ipxe/efi/Protocol/SimpleFileSystem.h>
#include <ipxe/efi/Protocol/AcpiTable.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/efi_strings.h>
#include <ipxe/efi/efi_snp.h>
#include <ipxe/efi/efi_utils.h>
#include <ipxe/efi/efi_block.h>

/** ACPI table protocol protocol */
static EFI_ACPI_TABLE_PROTOCOL *acpi;
EFI_REQUEST_PROTOCOL ( EFI_ACPI_TABLE_PROTOCOL, &acpi );

/** Boot filename */
static wchar_t efi_block_boot_filename[] = EFI_REMOVABLE_MEDIA_FILE_NAME;

/** iPXE EFI block device vendor device path GUID */
#define IPXE_BLOCK_DEVICE_PATH_GUID					\
	{ 0x8998b594, 0xf531, 0x4e87,					\
	  { 0x8b, 0xdf, 0x8f, 0x88, 0x54, 0x3e, 0x99, 0xd4 } }

/** iPXE EFI block device vendor device path GUID */
static EFI_GUID ipxe_block_device_path_guid
	= IPXE_BLOCK_DEVICE_PATH_GUID;

/** An iPXE EFI block device vendor device path */
struct efi_block_vendor_path {
	/** Generic vendor device path */
	VENDOR_DEVICE_PATH vendor;
	/** Block device URI */
	CHAR16 uri[0];
} __attribute__ (( packed ));

/** EFI SAN device private data */
struct efi_block_data {
	/** SAN device */
	struct san_device *sandev;
	/** EFI handle */
	EFI_HANDLE handle;
	/** Media descriptor */
	EFI_BLOCK_IO_MEDIA media;
	/** Block I/O protocol */
	EFI_BLOCK_IO_PROTOCOL block_io;
	/** Device path protocol */
	EFI_DEVICE_PATH_PROTOCOL *path;
};

/**
 * Read from or write to EFI block device
 *
 * @v sandev		SAN device
 * @v lba		Starting LBA
 * @v data		Data buffer
 * @v len		Size of buffer
 * @v sandev_rw		SAN device read/write method
 * @ret rc		Return status code
 */
static int efi_block_rw ( struct san_device *sandev, uint64_t lba,
			  void *data, size_t len,
			  int ( * sandev_rw ) ( struct san_device *sandev,
						uint64_t lba, unsigned int count,
						userptr_t buffer ) ) {
	struct efi_block_data *block = sandev->priv;
	unsigned int count;
	int rc;

	/* Sanity check */
	count = ( len / block->media.BlockSize );
	if ( ( count * block->media.BlockSize ) != len ) {
		DBGC ( sandev, "EFIBLK %#02x impossible length %#zx\n",
		       sandev->drive, len );
		return -EINVAL;
	}

	/* Read from / write to block device */
	if ( ( rc = sandev_rw ( sandev, lba, count,
				virt_to_user ( data ) ) ) != 0 ) {
		DBGC ( sandev, "EFIBLK %#02x I/O failed: %s\n",
		       sandev->drive, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Reset EFI block device
 *
 * @v block_io		Block I/O protocol
 * @v verify		Perform extended verification
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI efi_block_io_reset ( EFI_BLOCK_IO_PROTOCOL *block_io,
					      BOOLEAN verify __unused ) {
	struct efi_block_data *block =
		container_of ( block_io, struct efi_block_data, block_io );
	struct san_device *sandev = block->sandev;
	int rc;

	DBGC2 ( sandev, "EFIBLK %#02x reset\n", sandev->drive );
	efi_snp_claim();
	rc = sandev_reset ( sandev );
	efi_snp_release();
	return EFIRC ( rc );
}

/**
 * Read from EFI block device
 *
 * @v block_io		Block I/O protocol
 * @v media		Media identifier
 * @v lba		Starting LBA
 * @v len		Size of buffer
 * @v data		Data buffer
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_block_io_read ( EFI_BLOCK_IO_PROTOCOL *block_io, UINT32 media __unused,
		    EFI_LBA lba, UINTN len, VOID *data ) {
	struct efi_block_data *block =
		container_of ( block_io, struct efi_block_data, block_io );
	struct san_device *sandev = block->sandev;
	int rc;

	DBGC2 ( sandev, "EFIBLK %#02x read LBA %#08llx to %p+%#08zx\n",
		sandev->drive, lba, data, ( ( size_t ) len ) );
	efi_snp_claim();
	rc = efi_block_rw ( sandev, lba, data, len, sandev_read );
	efi_snp_release();
	return EFIRC ( rc );
}

/**
 * Write to EFI block device
 *
 * @v block_io		Block I/O protocol
 * @v media		Media identifier
 * @v lba		Starting LBA
 * @v len		Size of buffer
 * @v data		Data buffer
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_block_io_write ( EFI_BLOCK_IO_PROTOCOL *block_io, UINT32 media __unused,
		     EFI_LBA lba, UINTN len, VOID *data ) {
	struct efi_block_data *block =
		container_of ( block_io, struct efi_block_data, block_io );
	struct san_device *sandev = block->sandev;
	int rc;

	DBGC2 ( sandev, "EFIBLK %#02x write LBA %#08llx from %p+%#08zx\n",
		sandev->drive, lba, data, ( ( size_t ) len ) );
	efi_snp_claim();
	rc = efi_block_rw ( sandev, lba, data, len, sandev_write );
	efi_snp_release();
	return EFIRC ( rc );
}

/**
 * Flush data to EFI block device
 *
 * @v block_io		Block I/O protocol
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_block_io_flush ( EFI_BLOCK_IO_PROTOCOL *block_io ) {
	struct efi_block_data *block =
		container_of ( block_io, struct efi_block_data, block_io );
	struct san_device *sandev = block->sandev;

	DBGC2 ( sandev, "EFIBLK %#02x flush\n", sandev->drive );

	/* Nothing to do */
	return 0;
}

/**
 * Connect all possible drivers to EFI block device
 *
 * @v sandev		SAN device
 */
static void efi_block_connect ( struct san_device *sandev ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct efi_block_data *block = sandev->priv;
	EFI_STATUS efirc;
	int rc;

	/* Try to connect all possible drivers to this block device */
	if ( ( efirc = bs->ConnectController ( block->handle, NULL,
					       NULL, 1 ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( sandev, "EFIBLK %#02x could not connect drivers: %s\n",
		       sandev->drive, strerror ( rc ) );
		/* May not be an error; may already be connected */
	}
	DBGC2 ( sandev, "EFIBLK %#02x supports protocols:\n", sandev->drive );
	DBGC2_EFI_PROTOCOLS ( sandev, block->handle );
}

/**
 * Hook EFI block device
 *
 * @v drive		Drive number
 * @v uris		List of URIs
 * @v count		Number of URIs
 * @v flags		Flags
 * @ret drive		Drive number, or negative error
 */
static int efi_block_hook ( unsigned int drive, struct uri **uris,
			    unsigned int count, unsigned int flags ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_DEVICE_PATH_PROTOCOL *end;
	struct efi_block_vendor_path *vendor;
	struct efi_snp_device *snpdev;
	struct san_device *sandev;
	struct efi_block_data *block;
	size_t prefix_len;
	size_t uri_len;
	size_t vendor_len;
	size_t len;
	char *uri_buf;
	EFI_STATUS efirc;
	int rc;

	/* Sanity check */
	if ( ! count ) {
		DBG ( "EFIBLK has no URIs\n" );
		rc = -ENOTTY;
		goto err_no_uris;
	}

	/* Find an appropriate parent device handle */
	snpdev = last_opened_snpdev();
	if ( ! snpdev ) {
		DBG ( "EFIBLK could not identify SNP device\n" );
		rc = -ENODEV;
		goto err_no_snpdev;
	}

	/* Calculate length of private data */
	prefix_len = efi_devpath_len ( snpdev->path );
	uri_len = format_uri ( uris[0], NULL, 0 );
	vendor_len = ( sizeof ( *vendor ) +
		       ( ( uri_len + 1 /* NUL */ ) * sizeof ( wchar_t ) ) );
	len = ( sizeof ( *block ) + uri_len + 1 /* NUL */ + prefix_len +
		vendor_len + sizeof ( *end ) );

	/* Allocate and initialise structure */
	sandev = alloc_sandev ( uris, count, len );
	if ( ! sandev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	block = sandev->priv;
	block->sandev = sandev;
	block->media.MediaPresent = 1;
	block->media.LogicalBlocksPerPhysicalBlock = 1;
	block->block_io.Revision = EFI_BLOCK_IO_PROTOCOL_REVISION3;
	block->block_io.Media = &block->media;
	block->block_io.Reset = efi_block_io_reset;
	block->block_io.ReadBlocks = efi_block_io_read;
	block->block_io.WriteBlocks = efi_block_io_write;
	block->block_io.FlushBlocks = efi_block_io_flush;
	uri_buf = ( ( ( void * ) block ) + sizeof ( *block ) );
	block->path = ( ( ( void * ) uri_buf ) + uri_len + 1 /* NUL */ );

	/* Construct device path */
	memcpy ( block->path, snpdev->path, prefix_len );
	vendor = ( ( ( void * ) block->path ) + prefix_len );
	vendor->vendor.Header.Type = HARDWARE_DEVICE_PATH;
	vendor->vendor.Header.SubType = HW_VENDOR_DP;
	vendor->vendor.Header.Length[0] = ( vendor_len & 0xff );
	vendor->vendor.Header.Length[1] = ( vendor_len >> 8 );
	memcpy ( &vendor->vendor.Guid, &ipxe_block_device_path_guid,
		 sizeof ( vendor->vendor.Guid ) );
	format_uri ( uris[0], uri_buf, ( uri_len + 1 /* NUL */ ) );
	efi_snprintf ( vendor->uri, ( uri_len + 1 /* NUL */ ), "%s", uri_buf );
	end = ( ( ( void * ) vendor ) + vendor_len );
	end->Type = END_DEVICE_PATH_TYPE;
	end->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
	end->Length[0] = sizeof ( *end );
	DBGC ( sandev, "EFIBLK %#02x has device path %s\n",
	       drive, efi_devpath_text ( block->path ) );

	/* Register SAN device */
	if ( ( rc = register_sandev ( sandev, drive, flags ) ) != 0 ) {
		DBGC ( sandev, "EFIBLK %#02x could not register: %s\n",
		       drive, strerror ( rc ) );
		goto err_register;
	}

	/* Update media descriptor */
	block->media.BlockSize =
		( sandev->capacity.blksize << sandev->blksize_shift );
	block->media.LastBlock =
		( ( sandev->capacity.blocks >> sandev->blksize_shift ) - 1 );

	/* Install protocols */
	if ( ( efirc = bs->InstallMultipleProtocolInterfaces (
			&block->handle,
			&efi_block_io_protocol_guid, &block->block_io,
			&efi_device_path_protocol_guid, block->path,
			NULL ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( sandev, "EFIBLK %#02x could not install protocols: %s\n",
		       sandev->drive, strerror ( rc ) );
		goto err_install;
	}

	/* Connect all possible protocols */
	efi_block_connect ( sandev );

	return drive;

	bs->UninstallMultipleProtocolInterfaces (
			block->handle,
			&efi_block_io_protocol_guid, &block->block_io,
			&efi_device_path_protocol_guid, block->path, NULL );
 err_install:
	unregister_sandev ( sandev );
 err_register:
	sandev_put ( sandev );
 err_alloc:
 err_no_snpdev:
 err_no_uris:
	return rc;
}

/**
 * Unhook EFI block device
 *
 * @v drive		Drive number
 */
static void efi_block_unhook ( unsigned int drive ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct san_device *sandev;
	struct efi_block_data *block;

	/* Find SAN device */
	sandev = sandev_find ( drive );
	if ( ! sandev ) {
		DBG ( "EFIBLK cannot find drive %#02x\n", drive );
		return;
	}
	block = sandev->priv;

	/* Uninstall protocols */
	bs->UninstallMultipleProtocolInterfaces (
			block->handle,
			&efi_block_io_protocol_guid, &block->block_io,
			&efi_device_path_protocol_guid, block->path, NULL );

	/* Unregister SAN device */
	unregister_sandev ( sandev );

	/* Drop reference to drive */
	sandev_put ( sandev );
}

/** An installed ACPI table */
struct efi_acpi_table {
	/** List of installed tables */
	struct list_head list;
	/** Table key */
	UINTN key;
};

/** List of installed ACPI tables */
static LIST_HEAD ( efi_acpi_tables );

/**
 * Install ACPI table
 *
 * @v hdr		ACPI description header
 * @ret rc		Return status code
 */
static int efi_block_install ( struct acpi_header *hdr ) {
	size_t len = le32_to_cpu ( hdr->length );
	struct efi_acpi_table *installed;
	EFI_STATUS efirc;
	int rc;

	/* Allocate installed table record */
	installed = zalloc ( sizeof ( *installed ) );
	if ( ! installed ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Fill in common parameters */
	strncpy ( hdr->oem_id, "FENSYS", sizeof ( hdr->oem_id ) );
	strncpy ( hdr->oem_table_id, "iPXE", sizeof ( hdr->oem_table_id ) );

	/* Fix up ACPI checksum */
	acpi_fix_checksum ( hdr );

	/* Install table */
	if ( ( efirc = acpi->InstallAcpiTable ( acpi, hdr, len,
						&installed->key ) ) != 0 ){
		rc = -EEFI ( efirc );
		DBGC ( acpi, "EFIBLK could not install %s: %s\n",
		       acpi_name ( hdr->signature ), strerror ( rc ) );
		DBGC_HDA ( acpi, 0, hdr, len );
		goto err_install;
	}

	/* Add to list of installed tables */
	list_add_tail ( &installed->list, &efi_acpi_tables );

	DBGC ( acpi, "EFIBLK installed %s as ACPI table %#lx:\n",
	       acpi_name ( hdr->signature ),
	       ( ( unsigned long ) installed->key ) );
	DBGC_HDA ( acpi, 0, hdr, len );
	return 0;

	list_del ( &installed->list );
 err_install:
	free ( installed );
 err_alloc:
	return rc;
}

/**
 * Describe EFI block devices
 *
 * @ret rc		Return status code
 */
static int efi_block_describe ( void ) {
	struct efi_acpi_table *installed;
	struct efi_acpi_table *tmp;
	UINTN key;
	EFI_STATUS efirc;
	int rc;

	/* Sanity check */
	if ( ! acpi ) {
		DBG ( "EFIBLK has no ACPI table protocol\n" );
		return -ENOTSUP;
	}

	/* Uninstall any existing ACPI tables */
	list_for_each_entry_safe ( installed, tmp, &efi_acpi_tables, list ) {
		key = installed->key;
		if ( ( efirc = acpi->UninstallAcpiTable ( acpi, key ) ) != 0 ) {
			rc = -EEFI ( efirc );
			DBGC ( acpi, "EFIBLK could not uninstall ACPI table "
			       "%#lx: %s\n", ( ( unsigned long ) key ),
			       strerror ( rc ) );
			/* Continue anyway */
		}
		list_del ( &installed->list );
		free ( installed );
	}

	/* Install ACPI tables */
	if ( ( rc = acpi_install ( efi_block_install ) ) != 0 )  {
		DBGC ( acpi, "EFIBLK could not install ACPI tables: %s\n",
		       strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Try booting from child device of EFI block device
 *
 * @v sandev		SAN device
 * @v handle		EFI handle
 * @v filename		Filename (or NULL to use default)
 * @v image		Image handle to fill in
 * @ret rc		Return status code
 */
static int efi_block_boot_image ( struct san_device *sandev, EFI_HANDLE handle,
				  const char *filename, EFI_HANDLE *image ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct efi_block_data *block = sandev->priv;
	union {
		EFI_DEVICE_PATH_PROTOCOL *path;
		void *interface;
	} path;
	EFI_DEVICE_PATH_PROTOCOL *boot_path;
	FILEPATH_DEVICE_PATH *filepath;
	EFI_DEVICE_PATH_PROTOCOL *end;
	size_t prefix_len;
	size_t filepath_len;
	size_t boot_path_len;
	EFI_STATUS efirc;
	int rc;

	/* Identify device path */
	if ( ( efirc = bs->OpenProtocol ( handle,
					  &efi_device_path_protocol_guid,
					  &path.interface, efi_image_handle,
					  handle,
					  EFI_OPEN_PROTOCOL_GET_PROTOCOL ))!=0){
		DBGC ( sandev, "EFIBLK %#02x found filesystem with no device "
		       "path??", sandev->drive );
		rc = -EEFI ( efirc );
		goto err_open_device_path;
	}

	/* Check if this device is a child of our block device */
	prefix_len = efi_devpath_len ( block->path );
	if ( memcmp ( path.path, block->path, prefix_len ) != 0 ) {
		/* Not a child device */
		rc = -ENOTTY;
		goto err_not_child;
	}
	DBGC ( sandev, "EFIBLK %#02x found child device %s\n",
	       sandev->drive, efi_devpath_text ( path.path ) );

	/* Construct device path for boot image */
	end = efi_devpath_end ( path.path );
	prefix_len = ( ( ( void * ) end ) - ( ( void * ) path.path ) );
	filepath_len = ( SIZE_OF_FILEPATH_DEVICE_PATH +
			 ( filename ?
			   ( ( strlen ( filename ) + 1 /* NUL */ ) *
			     sizeof ( filepath->PathName[0] ) ) :
			   sizeof ( efi_block_boot_filename ) ) );
	boot_path_len = ( prefix_len + filepath_len + sizeof ( *end ) );
	boot_path = zalloc ( boot_path_len );
	if ( ! boot_path ) {
		rc = -ENOMEM;
		goto err_alloc_path;
	}
	memcpy ( boot_path, path.path, prefix_len );
	filepath = ( ( ( void * ) boot_path ) + prefix_len );
	filepath->Header.Type = MEDIA_DEVICE_PATH;
	filepath->Header.SubType = MEDIA_FILEPATH_DP;
	filepath->Header.Length[0] = ( filepath_len & 0xff );
	filepath->Header.Length[1] = ( filepath_len >> 8 );
	if ( filename ) {
		efi_sprintf ( filepath->PathName, "%s", filename );
	} else {
		memcpy ( filepath->PathName, efi_block_boot_filename,
			 sizeof ( efi_block_boot_filename ) );
	}
	end = ( ( ( void * ) filepath ) + filepath_len );
	end->Type = END_DEVICE_PATH_TYPE;
	end->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
	end->Length[0] = sizeof ( *end );
	DBGC ( sandev, "EFIBLK %#02x trying to load %s\n",
	       sandev->drive, efi_devpath_text ( boot_path ) );

	/* Try loading boot image from this device */
	if ( ( efirc = bs->LoadImage ( FALSE, efi_image_handle, boot_path,
				       NULL, 0, image ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( sandev, "EFIBLK %#02x could not load image: %s\n",
		       sandev->drive, strerror ( rc ) );
		goto err_load_image;
	}

	/* Success */
	rc = 0;

 err_load_image:
	free ( boot_path );
 err_alloc_path:
 err_not_child:
 err_open_device_path:
	return rc;
}

/**
 * Boot from EFI block device
 *
 * @v drive		Drive number
 * @v filename		Filename (or NULL to use default)
 * @ret rc		Return status code
 */
static int efi_block_boot ( unsigned int drive, const char *filename ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct san_device *sandev;
	EFI_HANDLE *handles;
	EFI_HANDLE image = NULL;
	UINTN count;
	unsigned int i;
	EFI_STATUS efirc;
	int rc;

	/* Find SAN device */
	sandev = sandev_find ( drive );
	if ( ! sandev ) {
		DBG ( "EFIBLK cannot find drive %#02x\n", drive );
		rc = -ENODEV;
		goto err_sandev_find;
	}

	/* Connect all possible protocols */
	efi_block_connect ( sandev );

	/* Locate all handles supporting the Simple File System protocol */
	if ( ( efirc = bs->LocateHandleBuffer (
			ByProtocol, &efi_simple_file_system_protocol_guid,
			NULL, &count, &handles ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( sandev, "EFIBLK %#02x cannot locate file systems: %s\n",
		       sandev->drive, strerror ( rc ) );
		goto err_locate_file_systems;
	}

	/* Try booting from any available child device containing a
	 * suitable boot image.  This is something of a wild stab in
	 * the dark, but should end up conforming to user expectations
	 * most of the time.
	 */
	rc = -ENOENT;
	for ( i = 0 ; i < count ; i++ ) {
		if ( ( rc = efi_block_boot_image ( sandev, handles[i], filename,
						   &image ) ) != 0 )
			continue;
		DBGC ( sandev, "EFIBLK %#02x found boot image\n",
		       sandev->drive );
		efirc = bs->StartImage ( image, NULL, NULL );
		rc = ( efirc ? -EEFI ( efirc ) : 0 );
		bs->UnloadImage ( image );
		DBGC ( sandev, "EFIBLK %#02x boot image returned: %s\n",
		       sandev->drive, strerror ( rc ) );
		break;
	}

	bs->FreePool ( handles );
 err_locate_file_systems:
 err_sandev_find:
	return rc;
}

PROVIDE_SANBOOT ( efi, san_hook, efi_block_hook );
PROVIDE_SANBOOT ( efi, san_unhook, efi_block_unhook );
PROVIDE_SANBOOT ( efi, san_describe, efi_block_describe );
PROVIDE_SANBOOT ( efi, san_boot, efi_block_boot );
