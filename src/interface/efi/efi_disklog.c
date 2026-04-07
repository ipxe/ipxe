/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
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
FILE_SECBOOT ( PERMITTED );

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <ipxe/console.h>
#include <ipxe/disklog.h>
#include <ipxe/init.h>
#include <ipxe/umalloc.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/BlockIo.h>
#include <ipxe/efi/Protocol/PartitionInfo.h>
#include <config/console.h>

/** @file
 *
 * EFI disk log console
 *
 */

/* Set default console usage if applicable */
#if ! ( defined ( CONSOLE_DISKLOG ) && CONSOLE_EXPLICIT ( CONSOLE_DISKLOG ) )
#undef CONSOLE_DISKLOG
#define CONSOLE_DISKLOG ( CONSOLE_USAGE_ALL & ~CONSOLE_USAGE_LOG )
#endif

/** EFI disk log console device handle */
static EFI_HANDLE efi_disklog_handle;

/** EFI disk log console block I/O protocol */
static EFI_BLOCK_IO_PROTOCOL *efi_disklog_block;

/** EFI disk log console media ID */
static UINT32 efi_disklog_media_id;

/** EFI disk log console */
static struct disklog efi_disklog;

struct console_driver efi_disklog_console __console_driver;

/**
 * Write current logical block
 *
 * @ret rc		Return status code
 */
static int efi_disklog_write ( void ) {
	EFI_BLOCK_IO_PROTOCOL *block = efi_disklog_block;
	struct disklog *disklog = &efi_disklog;
	EFI_STATUS efirc;
	int rc;

	/* Write disk block */
	if ( ( efirc = block->WriteBlocks ( block, efi_disklog_media_id,
					    disklog->lba, disklog->blksize,
					    disklog->buffer ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( disklog, "EFIDISKLOG %s could not write LBA %#llx: "
		       "%s\n", efi_handle_name ( efi_disklog_handle ),
		       ( ( unsigned long long ) disklog->lba ),
		       strerror ( rc ) );
		return rc;
	}

	return 0;
}

/** EFI disk log console operations */
static struct disklog_operations efi_disklog_op = {
	.write = efi_disklog_write,
};

/**
 * Write character to console
 *
 * @v character		Character
 */
static void efi_disklog_putchar ( int character ) {

	/* Write character */
	disklog_putchar ( &efi_disklog, character );
}

/**
 * Open EFI disk log partition
 *
 * @v handle		Block device handle
 * @ret rc		Return status code
 */
static int efi_disklog_open ( EFI_HANDLE handle ) {
	struct disklog *disklog = &efi_disklog;
	EFI_BLOCK_IO_PROTOCOL *block;
	EFI_BLOCK_IO_MEDIA *media;
	EFI_PARTITION_INFO_PROTOCOL *part;
	void *buffer;
	EFI_STATUS efirc;
	int rc;

	/* Record handle */
	efi_disklog_handle = handle;

	/* Open block I/O protocol for ephemeral usage */
	if ( ( rc = efi_open ( handle, &efi_block_io_protocol_guid,
			       &block ) ) != 0 ) {
		DBGC ( disklog, "EFIDISKLOG %s could not open: %s\n",
		       efi_handle_name ( handle ), strerror ( rc ) );
		goto err_open;
	}
	media = block->Media;
	efi_disklog_block = block;
	efi_disklog_media_id = media->MediaId;

	/* Check this is a partition */
	if ( ! media->LogicalPartition ) {
		DBGC2 ( disklog, "EFIDISKLOG %s is not a partition\n",
			efi_handle_name ( handle ) );
		rc = -ENOTTY;
		goto err_not_partition;
	}

	/* Check partition type (if exposed by the platform) */
	if ( ( rc = efi_open ( handle, &efi_partition_info_protocol_guid,
			       &part ) ) == 0 ) {
		if ( ( part->Type != PARTITION_TYPE_MBR ) ||
		     ( part->Info.Mbr.OSIndicator != DISKLOG_PARTITION_TYPE )){
			DBGC ( disklog, "EFIDISKLOG %s is not a log "
			       "partition\n", efi_handle_name ( handle ) );
			rc = -ENOTTY;
			goto err_not_log;
		}
	} else {
		DBGC2 ( disklog, "EFIDISKLOG %s has no partition info\n",
			efi_handle_name ( handle ) );
		/* Continue anyway */
	}

	/* Allocate buffer */
	buffer = umalloc ( media->BlockSize );
	if ( ! buffer ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Read partition signature */
	if ( ( efirc = block->ReadBlocks ( block, efi_disklog_media_id, 0,
					   media->BlockSize, buffer ) ) != 0 ){
		rc = -EEFI ( efirc );
		DBGC ( disklog, "EFIDISKLOG %s could not read block 0: %s\n",
		       efi_handle_name ( handle ), strerror ( rc ) );
		goto err_read;
	}

	/* Initialise disk log console */
	disklog_init ( disklog, &efi_disklog_op, &efi_disklog_console, buffer,
		       media->BlockSize, 0, media->LastBlock );

	/* Open disk log console */
	if ( ( rc = disklog_open ( disklog ) ) != 0 ) {
		DBGC ( disklog, "EFIDISKLOG %s could not initialise log: "
		       "%s\n", efi_handle_name ( handle ), strerror ( rc ) );
		goto err_init;
	}

	/* Reopen handle for long-term use */
	if ( ( rc = efi_open_by_driver ( handle, &efi_block_io_protocol_guid,
					 &block ) ) != 0 ) {
		DBGC ( disklog, "EFIDISKLOG %s could not reopen: %s\n",
		       efi_handle_name ( handle ), strerror ( rc ) );
		goto err_reopen;
	}
	if ( block != efi_disklog_block ) {
		DBGC ( disklog, "EFIDISKLOG %s changed during reopening\n",
		       efi_handle_name ( handle ) );
		rc = -EPIPE;
		goto err_reopen_mismatch;
	}

	DBGC ( disklog, "EFIDISKLOG using %s\n", efi_handle_name ( handle ) );
	DBGC2 ( disklog, "EFIDISKLOG has %zd-byte LBA [%#x,%#llx]\n",
		disklog->blksize, 0,
		( ( unsigned long long ) disklog->max_lba ) );
	return 0;

 err_reopen_mismatch:
	efi_close_by_driver ( handle, &efi_block_io_protocol_guid );
 err_reopen:
 err_init:
 err_read:
	ufree ( buffer );
 err_alloc:
 err_not_log:
 err_not_partition:
	efi_disklog_block = NULL;
 err_open:
	efi_disklog_handle = NULL;
	return rc;
}

/**
 * Initialise EFI disk log console
 *
 */
static void efi_disklog_init ( void ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_GUID *protocol = &efi_block_io_protocol_guid;
	EFI_HANDLE *handles;
	UINTN count;
	unsigned int i;
	EFI_STATUS efirc;
	int rc;

	/* Locate all block I/O protocol handles */
	if ( ( efirc = bs->LocateHandleBuffer ( ByProtocol, protocol,
						NULL, &count,
						&handles ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( &efi_disklog, "EFIDISKLOG could not locate block I/O: "
		       "%s\n", strerror ( rc ) );
		goto err_locate;
	}

	/* Try each handle in turn */
	for ( i = 0 ; i < count ; i++ ) {
		if ( ( rc = efi_disklog_open ( handles[i] ) ) == 0 )
			break;
	}

	bs->FreePool ( handles );
 err_locate:
	return;
}

/**
 * EFI disk log console initialisation function
 */
struct init_fn efi_disklog_init_fn __init_fn ( INIT_CONSOLE ) = {
	.name = "disklog",
	.initialise = efi_disklog_init,
};

/**
 * Shut down EFI disk log console
 *
 * @v booting		System is shutting down for OS boot
 */
static void efi_disklog_shutdown ( int booting __unused ) {
	struct disklog *disklog = &efi_disklog;

	/* Do nothing if we have no EFI disk log console */
	if ( ! efi_disklog_handle )
		return;

	/* Close EFI disk log console */
	DBGC ( disklog, "EFIDISKLOG %s closed\n",
	       efi_handle_name ( efi_disklog_handle ) );
	efi_close_by_driver ( efi_disklog_handle,
			      &efi_block_io_protocol_guid );
	ufree ( disklog->buffer );
	disklog->buffer = NULL;
	efi_disklog_handle = NULL;
	efi_disklog_block = NULL;
}

/** EFI disk log console shutdown function */
struct startup_fn efi_disklog_startup_fn __startup_fn ( STARTUP_EARLY ) = {
	.name = "disklog",
	.shutdown = efi_disklog_shutdown,
};

/** EFI disk log console driver */
struct console_driver efi_disklog_console __console_driver = {
	.putchar = efi_disklog_putchar,
	.disabled = CONSOLE_DISABLED,
	.usage = CONSOLE_DISKLOG,
};

/* Request a log partition from genfsimg */
IPXE_NOTE ( DISKLOG );
