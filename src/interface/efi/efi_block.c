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
#include <stdio.h>
#include <string.h>
#include <strings.h>
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
#include <ipxe/efi/Guid/FileSystemInfo.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/efi_strings.h>
#include <ipxe/efi/efi_snp.h>
#include <ipxe/efi/efi_path.h>
#include <ipxe/efi/efi_null.h>
#include <ipxe/efi/efi_block.h>

/** ACPI table protocol protocol */
static EFI_ACPI_TABLE_PROTOCOL *acpi;
EFI_REQUEST_PROTOCOL ( EFI_ACPI_TABLE_PROTOCOL, &acpi );

/** Boot filename */
static wchar_t efi_block_boot_filename[] = EFI_REMOVABLE_MEDIA_FILE_NAME;

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
						uint64_t lba,
						unsigned int count,
						void *buffer ) ) {
	struct efi_block_data *block = sandev->priv;
	unsigned int count;
	int rc;

	/* Sanity check */
	count = ( len / block->media.BlockSize );
	if ( ( count * block->media.BlockSize ) != len ) {
		DBGC ( sandev->drive, "EFIBLK %#02x impossible length %#zx\n",
		       sandev->drive, len );
		return -EINVAL;
	}

	/* Read from / write to block device */
	if ( ( rc = sandev_rw ( sandev, lba, count, data ) ) != 0 ) {
		DBGC ( sandev->drive, "EFIBLK %#02x I/O failed: %s\n",
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

	DBGC2 ( sandev->drive, "EFIBLK %#02x reset\n", sandev->drive );
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

	DBGC2 ( sandev->drive, "EFIBLK %#02x read LBA %#08llx to %p+%#08zx\n",
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

	DBGC2 ( sandev->drive, "EFIBLK %#02x write LBA %#08llx from "
		"%p+%#08zx\n", sandev->drive, lba, data, ( ( size_t ) len ) );
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

	DBGC2 ( sandev->drive, "EFIBLK %#02x flush\n", sandev->drive );

	/* Nothing to do */
	return 0;
}

/**
 * Connect all possible drivers to EFI block device
 *
 * @v drive		Drive number
 * @v handle		Block device handle
 */
static void efi_block_connect ( unsigned int drive, EFI_HANDLE handle ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_STATUS efirc;
	int rc;

	/* Try to connect all possible drivers to this block device */
	if ( ( efirc = bs->ConnectController ( handle, NULL, NULL,
					       TRUE ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( drive, "EFIBLK %#02x could not connect drivers: %s\n",
		       drive, strerror ( rc ) );
		/* May not be an error; may already be connected */
	}
	DBGC2 ( drive, "EFIBLK %#02x supports protocols:\n", drive );
	DBGC2_EFI_PROTOCOLS ( drive, handle );
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
	struct san_device *sandev;
	struct efi_block_data *block;
	int leak = 0;
	EFI_STATUS efirc;
	int rc;

	/* Sanity check */
	if ( ! count ) {
		DBGC ( drive, "EFIBLK %#02x has no URIs\n", drive );
		rc = -ENOTTY;
		goto err_no_uris;
	}

	/* Allocate and initialise structure */
	sandev = alloc_sandev ( uris, count, sizeof ( *block ) );
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

	/* Register SAN device */
	if ( ( rc = register_sandev ( sandev, drive, flags ) ) != 0 ) {
		DBGC ( drive, "EFIBLK %#02x could not register: %s\n",
		       drive, strerror ( rc ) );
		goto err_register;
	}

	/* Update media descriptor */
	block->media.BlockSize =
		( sandev->capacity.blksize << sandev->blksize_shift );
	block->media.LastBlock =
		( ( sandev->capacity.blocks >> sandev->blksize_shift ) - 1 );

	/* Construct device path */
	if ( ! sandev->active ) {
		rc = -ENODEV;
		DBGC ( drive, "EFIBLK %#02x not active after registration\n",
		       drive );
		goto err_active;
	}
	block->path = efi_describe ( &sandev->active->block );
	if ( ! block->path ) {
		rc = -ENODEV;
		DBGC ( drive, "EFIBLK %#02x has no device path\n", drive );
		goto err_describe;
	}
	DBGC2 ( drive, "EFIBLK %#02x has device path %s\n",
		drive, efi_devpath_text ( block->path ) );

	/* Install protocols */
	if ( ( efirc = bs->InstallMultipleProtocolInterfaces (
			&block->handle,
			&efi_block_io_protocol_guid, &block->block_io,
			&efi_device_path_protocol_guid, block->path,
			NULL ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( drive, "EFIBLK %#02x could not install protocols: %s\n",
		       drive, strerror ( rc ) );
		goto err_install;
	}
	DBGC ( drive, "EFIBLK %#02x installed as SAN drive %s\n",
	       drive, efi_handle_name ( block->handle ) );

	/* Connect all possible protocols */
	efi_block_connect ( drive, block->handle );

	return drive;

	if ( ( efirc = bs->UninstallMultipleProtocolInterfaces (
			block->handle,
			&efi_block_io_protocol_guid, &block->block_io,
			&efi_device_path_protocol_guid, block->path,
			NULL ) ) != 0 ) {
		DBGC ( drive, "EFIBLK %#02x could not uninstall protocols: "
		       "%s\n", drive, strerror ( -EEFI ( efirc ) ) );
		leak = 1;
	}
	efi_nullify_block ( &block->block_io );
 err_install:
	if ( ! leak )  {
		free ( block->path );
		block->path = NULL;
	}
 err_describe:
 err_active:
	unregister_sandev ( sandev );
 err_register:
	if ( ! leak )
		sandev_put ( sandev );
 err_alloc:
 err_no_uris:
	if ( leak )
		DBGC ( drive, "EFIBLK %#02x nullified and leaked\n", drive );
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
	int leak = efi_shutdown_in_progress;
	EFI_STATUS efirc;

	/* Find SAN device */
	sandev = sandev_find ( drive );
	if ( ! sandev ) {
		DBGC ( drive, "EFIBLK %#02x is not a SAN drive\n", drive );
		return;
	}
	block = sandev->priv;

	/* Uninstall protocols */
	if ( ( ! efi_shutdown_in_progress ) &&
	     ( ( efirc = bs->UninstallMultipleProtocolInterfaces (
			block->handle,
			&efi_block_io_protocol_guid, &block->block_io,
			&efi_device_path_protocol_guid, block->path,
			NULL ) ) != 0 ) ) {
		DBGC ( drive, "EFIBLK %#02x could not uninstall protocols: "
		       "%s\n", drive, strerror ( -EEFI ( efirc ) ) );
		leak = 1;
	}
	efi_nullify_block ( &block->block_io );

	/* Free device path */
	if ( ! leak ) {
		free ( block->path );
		block->path = NULL;
	}

	/* Unregister SAN device */
	unregister_sandev ( sandev );

	/* Drop reference to drive */
	if ( ! leak )
		sandev_put ( sandev );

	/* Report leakage, if applicable */
	if ( leak && ( ! efi_shutdown_in_progress ) )
		DBGC ( drive, "EFIBLK %#02x nullified and leaked\n", drive );
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
		DBGC2_HDA ( acpi, 0, hdr, len );
		goto err_install;
	}

	/* Add to list of installed tables */
	list_add_tail ( &installed->list, &efi_acpi_tables );

	DBGC ( acpi, "EFIBLK installed %s as ACPI table %#lx\n",
	       acpi_name ( hdr->signature ),
	       ( ( unsigned long ) installed->key ) );
	DBGC2_HDA ( acpi, 0, hdr, len );
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
 * Open root directory within a filesystem
 *
 * @v drive		Drive number
 * @v handle		Filesystem handle
 * @v root		Root directory file to fill in
 * @ret rc		Return status code
 */
static int efi_block_root ( unsigned int drive, EFI_HANDLE handle,
			    EFI_FILE_PROTOCOL **root ) {
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
	EFI_STATUS efirc;
	int rc;

	/* Open filesystem protocol */
	if ( ( rc = efi_open ( handle, &efi_simple_file_system_protocol_guid,
			       &fs ) ) != 0 ) {
		DBGC ( drive, "EFIBLK %#02x could not open %s filesystem: %s\n",
		       drive, efi_handle_name ( handle ), strerror ( rc ) );
		return rc;
	}

	/* Open root volume */
	if ( ( efirc = fs->OpenVolume ( fs, root ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( drive, "EFIBLK %#02x could not open %s root: %s\n",
		       drive, efi_handle_name ( handle ), strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Check for existence of a file within a filesystem
 *
 * @v drive		Drive number
 * @v handle		Filesystem handle
 * @v root		Root directory
 * @v filename		Filename (or NULL to use default)
 * @ret rc		Return status code
 */
static int efi_block_filename ( unsigned int drive, EFI_HANDLE handle,
				EFI_FILE_PROTOCOL *root,
				const char *filename ) {
	CHAR16 tmp[ filename ? ( strlen ( filename ) + 1 /* wNUL */ ) : 0 ];
	CHAR16 *wname;
	EFI_FILE_PROTOCOL *file;
	EFI_STATUS efirc;
	int rc;

	/* Construct filename */
	if ( filename ) {
		efi_snprintf ( tmp, sizeof ( tmp ), "%s", filename );
		wname = tmp;
	} else {
		wname = efi_block_boot_filename;
	}

	/* Try opening file */
	if ( ( efirc = root->Open ( root, &file, wname,
				    EFI_FILE_MODE_READ, 0 ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( drive, "EFIBLK %#02x could not open %s/%ls: %s\n",
		       drive, efi_handle_name ( handle ), wname,
		       strerror ( rc ) );
		goto err_file;
	}

	/* Success */
	rc = 0;

	file->Close ( file );
 err_file:
	return rc;
}

/**
 * Check for EFI block device filesystem label
 *
 * @v drive		Drive number
 * @v root		Root directory
 * @v label		Volume label
 * @ret rc		Return status code
 */
static int efi_block_label ( unsigned int drive, EFI_FILE_PROTOCOL *root,
			     const char *label ) {
	EFI_FILE_SYSTEM_INFO *info;
	UINTN size;
	char *actual;
	EFI_STATUS efirc;
	int rc;

	/* Get length of file system information */
	size = 0;
	root->GetInfo ( root, &efi_file_system_info_id, &size, NULL );

	/* Allocate file system information */
	info = malloc ( size );
	if ( ! info ) {
		rc = -ENOMEM;
		goto err_alloc_info;
	}

	/* Get file system information */
	if ( ( efirc = root->GetInfo ( root, &efi_file_system_info_id, &size,
				       info ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( drive, "EFIBLK %#02x could not get filesystem info: "
		       "%s\n", drive, strerror ( rc ) );
		goto err_get_info;
	}

	/* Construct volume label for comparison */
	if ( asprintf ( &actual, "%ls", info->VolumeLabel ) < 0 ) {
		rc = -ENOMEM;
		goto err_alloc_label;
	}

	/* Compare volume label */
	if ( strcasecmp ( label, actual ) != 0 ) {
		DBGC ( drive, "EFIBLK %#02x has wrong label \"%s\"\n",
		       drive, actual );
		rc = -ENOENT;
		goto err_compare;
	}

	/* Success */
	rc = 0;

 err_compare:
	free ( actual );
 err_alloc_label:
 err_get_info:
	free ( info );
 err_alloc_info:
	return rc;
}

/**
 * Check EFI block device filesystem match
 *
 * @v drive		Drive number
 * @v handle		Filesystem handle
 * @v path		Block device path
 * @v config		Boot configuration parameters
 * @v fspath		Filesystem device path to fill in
 * @ret rc		Return status code
 */
static int efi_block_match ( unsigned int drive, EFI_HANDLE handle,
			     EFI_DEVICE_PATH_PROTOCOL *path,
			     struct san_boot_config *config,
			     EFI_DEVICE_PATH_PROTOCOL **fspath ) {
	EFI_FILE *root;
	union uuid guid;
	int rc;

	/* Identify device path */
	if ( ( rc = efi_open ( handle, &efi_device_path_protocol_guid,
			       fspath ) ) != 0 ) {
		DBGC ( drive, "EFIBLK %#02x could not open %s device path: "
		       "%s\n", drive, efi_handle_name ( handle ),
		       strerror ( rc ) );
		goto err_open;
	}

	/* Check if filesystem is a child of this block device */
	if ( memcmp ( *fspath, path, efi_path_len ( path ) ) != 0 ) {
		/* Not a child device */
		rc = -ENOTTY;
		DBGC2 ( drive, "EFIBLK %#02x is not parent of %s\n",
			drive, efi_handle_name ( handle ) );
		goto err_not_child;
	}
	DBGC ( drive, "EFIBLK %#02x contains filesystem %s\n",
	       drive, efi_devpath_text ( *fspath ) );

	/* Check if filesystem matches GUID, if applicable */
	if ( config->uuid ) {
		if ( ( rc = efi_path_guid ( *fspath, &guid ) ) != 0 ) {
			DBGC ( drive, "EFIBLK %#02x could not determine GUID: "
			       "%s\n", drive, strerror ( rc ) );
			goto err_no_guid;
		}
		if ( memcmp ( config->uuid, &guid, sizeof ( guid ) ) != 0 ) {
			DBGC ( drive, "EFIBLK %#02x has wrong GUID %s\n",
			       drive, uuid_ntoa ( &guid ) );
			rc = -ENOENT;
			goto err_wrong_guid;
		}
	}

	/* Open root directory */
	if ( ( rc = efi_block_root ( drive, handle, &root ) ) != 0 )
		goto err_root;

	/* Check if filesystem contains boot filename */
	if ( ( rc = efi_block_filename ( drive, handle, root,
					 config->filename ) ) != 0 ) {
		goto err_filename;
	}

	/* Check if filesystem contains additional filename, if applicable */
	if ( config->extra &&
	     ( ( rc = efi_block_filename ( drive, handle, root,
					   config->extra ) ) != 0 ) ) {
		goto err_extra;
	}

	/* Check volume label, if applicable */
	if ( config->label &&
	     ( ( rc = efi_block_label ( drive, root,
					config->label ) ) != 0 ) ) {
		goto err_label;
	}

	/* Success */
	rc = 0;

 err_label:
 err_extra:
 err_filename:
	root->Close ( root );
 err_root:
 err_wrong_guid:
 err_no_guid:
 err_not_child:
 err_open:
	return rc;
}

/**
 * Scan EFI block device for a matching filesystem
 *
 * @v drive		Drive number
 * @v handle		Block device handle
 * @v config		Boot configuration parameters
 * @v fspath		Filesystem device path to fill in
 * @ret rc		Return status code
 */
static int efi_block_scan ( unsigned int drive, EFI_HANDLE handle,
			    struct san_boot_config *config,
			    EFI_DEVICE_PATH_PROTOCOL **fspath ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_DEVICE_PATH_PROTOCOL *path;
	EFI_HANDLE *handles;
	UINTN count;
	unsigned int i;
	EFI_STATUS efirc;
	int rc;

	/* Connect up possible file system drivers */
	efi_block_connect ( drive, handle );

	/* Identify device path */
	if ( ( rc = efi_open ( handle, &efi_device_path_protocol_guid,
			       &path ) ) != 0 ) {
		DBGC ( drive, "EFIBLK %#02x could not open device path: %s\n",
		       drive, strerror ( rc ) );
		goto err_open;
	}

	/* Locate all Simple File System protocol handles */
	if ( ( efirc = bs->LocateHandleBuffer (
			ByProtocol, &efi_simple_file_system_protocol_guid,
			NULL, &count, &handles ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( drive, "EFIBLK %#02x cannot locate file systems: %s\n",
		       drive, strerror ( rc ) );
		goto err_locate;
	}

	/* Scan for a matching filesystem */
	rc = -ENOENT;
	for ( i = 0 ; i < count ; i++ ) {

		/* Check for a matching filesystem */
		if ( ( rc = efi_block_match ( drive, handles[i], path,
					      config, fspath ) ) != 0 )
			continue;

		break;
	}

	bs->FreePool ( handles );
 err_locate:
 err_open:
	return rc;
}

/**
 * Boot from EFI block device filesystem boot image
 *
 * @v drive		Drive number
 * @v fspath		Filesystem device path
 * @v filename		Filename (or NULL to use default)
 * @ret rc		Return status code
 */
static int efi_block_exec ( unsigned int drive,
			    EFI_DEVICE_PATH_PROTOCOL *fspath,
			    const char *filename ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_DEVICE_PATH_PROTOCOL *path;
	FILEPATH_DEVICE_PATH *filepath;
	EFI_DEVICE_PATH_PROTOCOL *end;
	EFI_HANDLE image;
	size_t fspath_len;
	size_t filepath_len;
	size_t path_len;
	EFI_STATUS efirc;
	int rc;

	/* Construct device path for boot image */
	end = efi_path_end ( fspath );
	fspath_len = ( ( ( void * ) end ) - ( ( void * ) fspath ) );
	filepath_len = ( SIZE_OF_FILEPATH_DEVICE_PATH +
			 ( filename ?
			   ( ( strlen ( filename ) + 1 /* NUL */ ) *
			     sizeof ( filepath->PathName[0] ) ) :
			   sizeof ( efi_block_boot_filename ) ) );
	path_len = ( fspath_len + filepath_len + sizeof ( *end ) );
	path = zalloc ( path_len );
	if ( ! path ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	memcpy ( path, fspath, fspath_len );
	filepath = ( ( ( void * ) path ) + fspath_len );
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
	efi_path_terminate ( end );
	DBGC ( drive, "EFIBLK %#02x trying to load %s\n",
	       drive, efi_devpath_text ( path ) );

	/* Load image */
	image = NULL;
	if ( ( efirc = bs->LoadImage ( FALSE, efi_image_handle, path, NULL, 0,
				       &image ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( drive, "EFIBLK %#02x could not load: %s\n",
		       drive, strerror ( rc ) );
		if ( efirc == EFI_SECURITY_VIOLATION ) {
			goto err_load_security_violation;
		} else {
			goto err_load;
		}
	}

	/* Start image */
	efirc = bs->StartImage ( image, NULL, NULL );
	rc = ( efirc ? -EEFI ( efirc ) : 0 );
	DBGC ( drive, "EFIBLK %#02x boot image returned: %s\n",
	       drive, strerror ( rc ) );

 err_load_security_violation:
	bs->UnloadImage ( image );
 err_load:
	free ( path );
 err_alloc:
	return rc;
}

/**
 * Check that EFI block device is eligible for a local virtual drive number
 *
 * @v handle		Block device handle
 * @ret rc		Return status code
 *
 * We assign virtual drive numbers for local (non-SAN) EFI block
 * devices that represent complete disks, to provide roughly
 * equivalent functionality to BIOS drive numbers.
 */
static int efi_block_local ( EFI_HANDLE handle ) {
	struct san_device *sandev;
	struct efi_block_data *block;
	EFI_BLOCK_IO_PROTOCOL *blockio;
	int rc;

	/* Check if handle belongs to a SAN device */
	for_each_sandev ( sandev ) {
		block = sandev->priv;
		if ( handle == block->handle )
			return -ENOTTY;
	}

	/* Open block I/O protocol */
	if ( ( rc = efi_open ( handle, &efi_block_io_protocol_guid,
			       &blockio ) ) != 0 ) {
		DBGC ( handle, "EFIBLK %s could not open block I/O: %s\n",
		       efi_handle_name ( handle ), strerror ( rc ) );
		return rc;
	}

	/* Do not assign drive numbers for partitions */
	if ( blockio->Media->LogicalPartition ) {
		DBGC2 ( handle, "EFLBLK %s is a partition\n",
			efi_handle_name ( handle ) );
		return -ENOTTY;
	}

	return 0;
}

/**
 * Boot from EFI block device
 *
 * @v drive		Drive number
 * @v config		Boot configuration parameters
 * @ret rc		Return status code
 */
static int efi_block_boot ( unsigned int drive,
			    struct san_boot_config *config ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_DEVICE_PATH_PROTOCOL *fspath = NULL;
	EFI_HANDLE *handles;
	EFI_HANDLE handle;
	UINTN count;
	struct san_device *sandev;
	struct efi_block_data *block;
	unsigned int vdrive;
	unsigned int index;
	EFI_STATUS efirc;
	int rc;

	/* Ensure that any local drives are connected */
	efi_driver_reconnect_all();

	/* Release SNP devices */
	efi_snp_release();

	/* Locate all block I/O protocol handles */
	if ( ( efirc = bs->LocateHandleBuffer ( ByProtocol,
						&efi_block_io_protocol_guid,
						NULL, &count,
						&handles ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( drive, "EFIBLK %#02x cannot locate block I/O: %s\n",
		       drive, strerror ( rc ) );
		goto err_locate_block_io;
	}

	/* Try booting from the first matching block device, if any */
	rc = -ENOENT;
	for ( vdrive = 0, index = 0 ; ; vdrive++ ) {

		/* Identify next drive number and block I/O handle */
		if ( ( sandev = sandev_next ( vdrive ) ) &&
		     ( ( sandev->drive == vdrive ) ||
		       ( sandev->drive <= SAN_DEFAULT_DRIVE ) ||
		       ( index >= count ) ) ) {

			/* There is a SAN drive that either:
			 *
			 * a) has the current virtual drive number, or
			 * b) has a drive number below SAN_DEFAULT_DRIVE, or
			 * c) has a drive number higher than any local drive
			 *
			 * Use this SAN drive, since the explicit SAN
			 * drive numbering takes precedence over the
			 * implicit local drive numbering.
			 */
			block = sandev->priv;
			handle = block->handle;

			/* Use SAN drive's explicit drive number */
			vdrive = sandev->drive;
			DBGC ( vdrive, "EFIBLK %#02x is SAN drive %s\n",
			       vdrive, efi_handle_name ( handle ) );

		} else if ( index < count ) {

			/* There is no SAN drive meeting any of the
			 * above criteria.  Try the next block I/O
			 * handle.
			 */
			handle = handles[index++];

			/* Check if this handle is eligible to be
			 * given a local virtual drive number.
			 *
			 * Do not record this as the overall error
			 * status, since it is not an interesting
			 * error.
			 */
			if ( efi_block_local ( handle ) != 0 ) {
				/* Do not consume virtual drive number */
				vdrive--;
				continue;
			}

			/* Use the current virtual drive number, with
			 * a minimum of SAN_DEFAULT_DRIVE to match
			 * typical BIOS drive numbering.
			 */
			if ( vdrive < SAN_DEFAULT_DRIVE )
				vdrive = SAN_DEFAULT_DRIVE;
			DBGC ( vdrive, "EFIBLK %#02x is local drive %s\n",
			       vdrive, efi_handle_name ( handle ) );

		} else {

			/* No more SAN or local drives */
			break;
		}

		/* Skip non-matching drives */
		if ( drive && ( drive != vdrive ) )
			continue;
		DBGC ( vdrive, "EFIBLK %#02x attempting to boot\n", vdrive );

		/* Scan for a matching filesystem within this drive */
		if ( ( rc = efi_block_scan ( vdrive, handle, config,
					     &fspath ) ) != 0 ) {
			continue;
		}

		/* Attempt to boot from the matched filesystem */
		rc = efi_block_exec ( vdrive, fspath, config->filename );
		break;
	}

	bs->FreePool ( handles );
 err_locate_block_io:
	efi_snp_claim();
	return rc;
}

PROVIDE_SANBOOT ( efi, san_hook, efi_block_hook );
PROVIDE_SANBOOT ( efi, san_unhook, efi_block_unhook );
PROVIDE_SANBOOT ( efi, san_describe, efi_block_describe );
PROVIDE_SANBOOT ( efi, san_boot, efi_block_boot );
