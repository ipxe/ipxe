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
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/BlockIo.h>
#include <ipxe/efi/Protocol/SimpleFileSystem.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/efi_strings.h>
#include <ipxe/efi/efi_snp.h>
#include <ipxe/efi/efi_utils.h>
#include <ipxe/efi/efi_block.h>

/** Timeout for EFI block device commands (in ticks) */
#define EFI_BLOCK_TIMEOUT ( 15 * TICKS_PER_SEC )

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

/** An EFI block device */
struct efi_block {
	/** Reference count */
	struct refcnt refcnt;
	/** List of all registered block devices */
	struct list_head list;

	/** Block device URI */
	struct uri *uri;
	/** Drive number */
	unsigned int drive;
	/** Underlying block device interface */
	struct interface intf;

	/** Current device status */
	int block_rc;
	/** Raw block device capacity */
	struct block_device_capacity capacity;
	/** Block size shift
	 *
	 * To allow for emulation of CD-ROM access, this represents
	 * the left-shift required to translate from EFI I/O blocks to
	 * underlying blocks.
	 *
	 * Note that the LogicalBlocksPerPhysicalBlock field in
	 * EFI_BLOCK_IO_MEDIA is unable to encapsulate this
	 * information, since it does not allow us to describe a
	 * situation in which there are multiple "physical"
	 * (i.e. underlying) blocks per logical (i.e. exposed) block.
	 */
	unsigned int blksize_shift;

	/** EFI handle */
	EFI_HANDLE handle;
	/** Media descriptor */
	EFI_BLOCK_IO_MEDIA media;
	/** Block I/O protocol */
	EFI_BLOCK_IO_PROTOCOL block_io;
	/** Device path protocol */
	EFI_DEVICE_PATH_PROTOCOL *path;

	/** Command interface */
	struct interface command;
	/** Command timeout timer */
	struct retry_timer timer;
	/** Command status */
	int command_rc;
};

/**
 * Free EFI block device
 *
 * @v refcnt		Reference count
 */
static void efi_block_free ( struct refcnt *refcnt ) {
	struct efi_block *block =
		container_of ( refcnt, struct efi_block, refcnt );

	assert ( ! timer_running ( &block->timer ) );
	uri_put ( block->uri );
	free ( block->path );
	free ( block );
}

/** List of EFI block devices */
static LIST_HEAD ( efi_block_devices );

/**
 * Find EFI block device
 *
 * @v drive		Drive number
 * @ret block		Block device, or NULL if not found
 */
static struct efi_block * efi_block_find ( unsigned int drive ) {
	struct efi_block *block;

	list_for_each_entry ( block, &efi_block_devices, list ) {
		if ( block->drive == drive )
			return block;
	}

	return NULL;
}

/**
 * Close EFI block device command
 *
 * @v block		Block device
 * @v rc		Reason for close
 */
static void efi_block_cmd_close ( struct efi_block *block, int rc ) {

	/* Stop timer */
	stop_timer ( &block->timer );

	/* Restart interface */
	intf_restart ( &block->command, rc );

	/* Record command status */
	block->command_rc = rc;
}

/**
 * Record EFI block device capacity
 *
 * @v block		Block device
 * @v capacity		Block device capacity
 */
static void efi_block_cmd_capacity ( struct efi_block *block,
				     struct block_device_capacity *capacity ) {

	/* Record raw capacity information */
	memcpy ( &block->capacity, capacity, sizeof ( block->capacity ) );
}

/** EFI block device command interface operations */
static struct interface_operation efi_block_cmd_op[] = {
	INTF_OP ( intf_close, struct efi_block *, efi_block_cmd_close ),
	INTF_OP ( block_capacity, struct efi_block *, efi_block_cmd_capacity ),
};

/** EFI block device command interface descriptor */
static struct interface_descriptor efi_block_cmd_desc =
	INTF_DESC ( struct efi_block, command, efi_block_cmd_op );

/**
 * Handle EFI block device command timeout
 *
 * @v retry		Retry timer
 */
static void efi_block_cmd_expired ( struct retry_timer *timer,
				    int over __unused ) {
	struct efi_block *block =
		container_of ( timer, struct efi_block, timer );

	efi_block_cmd_close ( block, -ETIMEDOUT );
}

/**
 * Restart EFI block device interface
 *
 * @v block		Block device
 * @v rc		Reason for restart
 */
static void efi_block_restart ( struct efi_block *block, int rc ) {

	/* Restart block device interface */
	intf_nullify ( &block->command ); /* avoid potential loops */
	intf_restart ( &block->intf, rc );

	/* Close any outstanding command */
	efi_block_cmd_close ( block, rc );

	/* Record device error */
	block->block_rc = rc;
}

/**
 * (Re)open EFI block device
 *
 * @v block		Block device
 * @ret rc		Return status code
 *
 * This function will block until the device is available.
 */
static int efi_block_reopen ( struct efi_block *block ) {
	int rc;

	/* Close any outstanding command and restart interface */
	efi_block_restart ( block, -ECONNRESET );

	/* Mark device as being not yet open */
	block->block_rc = -EINPROGRESS;

	/* Open block device interface */
	if ( ( rc = xfer_open_uri ( &block->intf, block->uri ) ) != 0 ) {
		DBGC ( block, "EFIBLK %#02x could not (re)open URI: %s\n",
		       block->drive, strerror ( rc ) );
		return rc;
	}

	/* Wait for device to become available */
	while ( block->block_rc == -EINPROGRESS ) {
		step();
		if ( xfer_window ( &block->intf ) != 0 ) {
			block->block_rc = 0;
			return 0;
		}
	}

	DBGC ( block, "EFIBLK %#02x never became available: %s\n",
	       block->drive, strerror ( block->block_rc ) );
	return block->block_rc;
}

/**
 * Handle closure of underlying block device interface
 *
 * @v block		Block device
 * @ret rc		Reason for close
 */
static void efi_block_close ( struct efi_block *block, int rc ) {

	/* Any closure is an error from our point of view */
	if ( rc == 0 )
		rc = -ENOTCONN;
	DBGC ( block, "EFIBLK %#02x went away: %s\n",
	       block->drive, strerror ( rc ) );

	/* Close any outstanding command and restart interface */
	efi_block_restart ( block, rc );
}

/**
 * Check EFI block device flow control window
 *
 * @v block		Block device
 */
static size_t efi_block_window ( struct efi_block *block __unused ) {

	/* We are never ready to receive data via this interface.
	 * This prevents objects that support both block and stream
	 * interfaces from attempting to send us stream data.
	 */
	return 0;
}

/** EFI block device interface operations */
static struct interface_operation efi_block_op[] = {
	INTF_OP ( intf_close, struct efi_block *, efi_block_close ),
	INTF_OP ( xfer_window, struct efi_block *, efi_block_window ),
};

/** EFI block device interface descriptor */
static struct interface_descriptor efi_block_desc =
	INTF_DESC ( struct efi_block, intf, efi_block_op );

/** EFI block device command context */
struct efi_block_command_context {
	/** Starting LBA (using EFI block numbering) */
	uint64_t lba;
	/** Data buffer */
	void *data;
	/** Length of data buffer */
	size_t len;
	/** Block device read/write operation (if any) */
	int ( * block_rw ) ( struct interface *control, struct interface *data,
			     uint64_t lba, unsigned int count,
			     userptr_t buffer, size_t len );
};

/**
 * Initiate EFI block device command
 *
 * @v block		Block device
 * @v op		Command operation
 * @v context		Command context, or NULL if not required
 * @ret rc		Return status code
 */
static int efi_block_command ( struct efi_block *block,
			       int ( * op ) ( struct efi_block *block,
					      struct efi_block_command_context
						      *context ),
			       struct efi_block_command_context *context ) {
	int rc;

	/* Sanity check */
	assert ( ! timer_running ( &block->timer ) );

	/* Reopen block device if applicable */
	if ( ( block->block_rc != 0 ) &&
	     ( ( rc = efi_block_reopen ( block ) ) != 0 ) ) {
	     goto err_reopen;
	}

	/* Start expiry timer */
	start_timer_fixed ( &block->timer, EFI_BLOCK_TIMEOUT );

	/* Initiate block device operation */
	if ( ( rc = op ( block, context ) ) != 0 )
		goto err_op;

	/* Wait for command to complete */
	while ( timer_running ( &block->timer ) )
		step();

	/* Collect return status */
	rc = block->command_rc;

	return rc;

 err_op:
	stop_timer ( &block->timer );
 err_reopen:
	return rc;
}

/**
 * Initiate EFI block device read/write command
 *
 * @v block		Block device
 * @v context		Command context
 * @ret rc		Return status code
 */
static int efi_block_cmd_rw ( struct efi_block *block,
			      struct efi_block_command_context *context ) {
	uint64_t lba;
	unsigned int count;
	int rc;

	/* Calculate underlying starting LBA and block count */
	if ( block->capacity.blksize == 0 ) {
		DBGC ( block, "EFIBLK %#02x has zero block size\n",
		       block->drive );
		return -EINVAL;
	}
	lba = ( context->lba << block->blksize_shift );
	count = ( context->len / block->capacity.blksize );
	if ( ( count * block->capacity.blksize ) != context->len ) {
		DBGC ( block, "EFIBLK %#02x invalid read/write length %#zx\n",
		       block->drive, context->len );
		return -EINVAL;
	}

	/* Initiate read/write command */
	if ( ( rc = context->block_rw ( &block->intf, &block->command, lba,
					count, virt_to_user ( context->data ),
					context->len ) ) != 0 ) {
		DBGC ( block, "EFIBLK %#02x could not initiate read/write: "
		       "%s\n", block->drive, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Initiate EFI block device read capacity command
 *
 * @v block		Block device
 * @v context		Command context
 * @ret rc		Return status code
 */
static int efi_block_cmd_read_capacity ( struct efi_block *block,
					 struct efi_block_command_context
						 *context __unused ) {
	int rc;

	/* Initiate read capacity command */
	if ( ( rc = block_read_capacity ( &block->intf,
					  &block->command ) ) != 0 ) {
		DBGC ( block, "EFIBLK %#02x could not read capacity: %s\n",
		       block->drive, strerror ( rc ) );
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
	struct efi_block *block =
		container_of ( block_io, struct efi_block, block_io );
	int rc;

	DBGC2 ( block, "EFIBLK %#02x reset\n", block->drive );

	/* Claim network devices for use by iPXE */
	efi_snp_claim();

	/* Reopen block device */
	if ( ( rc = efi_block_reopen ( block ) ) != 0 )
		goto err_reopen;

 err_reopen:
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
	struct efi_block *block =
		container_of ( block_io, struct efi_block, block_io );
	struct efi_block_command_context context = {
		.lba = lba,
		.data = data,
		.len = len,
		.block_rw = block_read,
	};
	int rc;

	DBGC2 ( block, "EFIBLK %#02x read LBA %#08llx to %p+%#08zx\n",
		block->drive, context.lba, context.data, context.len );

	/* Claim network devices for use by iPXE */
	efi_snp_claim();

	/* Issue read command */
	if ( ( rc = efi_block_command ( block, efi_block_cmd_rw,
					&context ) ) != 0 )
		goto err_command;

 err_command:
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
	struct efi_block *block =
		container_of ( block_io, struct efi_block, block_io );
	struct efi_block_command_context context = {
		.lba = lba,
		.data = data,
		.len = len,
		.block_rw = block_write,
	};
	int rc;

	DBGC2 ( block, "EFIBLK %#02x write LBA %#08llx from %p+%#08zx\n",
		block->drive, context.lba, context.data, context.len );

	/* Claim network devices for use by iPXE */
	efi_snp_claim();

	/* Issue write command */
	if ( ( rc = efi_block_command ( block, efi_block_cmd_rw,
					&context ) ) != 0 )
		goto err_command;

 err_command:
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
	struct efi_block *block =
		container_of ( block_io, struct efi_block, block_io );

	DBGC2 ( block, "EFIBLK %#02x flush\n", block->drive );

	/* Nothing to do */
	return 0;
}

/**
 * Create device path for EFI block device
 *
 * @v uri		Block device URI
 * @v parent		Parent device path
 * @ret path		Device path, or NULL on failure
 *
 * The caller must eventually free() the device path.
 */
static EFI_DEVICE_PATH_PROTOCOL *
efi_block_path ( struct uri *uri, EFI_DEVICE_PATH_PROTOCOL *parent ) {
	EFI_DEVICE_PATH_PROTOCOL *path;
	struct efi_block_vendor_path *vendor;
	EFI_DEVICE_PATH_PROTOCOL *end;
	size_t prefix_len;
	size_t uri_len;
	size_t vendor_len;
	size_t len;
	char *uri_buf;

	/* Calculate device path lengths */
	end = efi_devpath_end ( parent );
	prefix_len = ( ( void * ) end - ( void * ) parent );
	uri_len = format_uri ( uri, NULL, 0 );
	vendor_len = ( sizeof ( *vendor ) +
		       ( ( uri_len + 1 /* NUL */ ) * sizeof ( wchar_t ) ) );
	len = ( prefix_len + vendor_len + sizeof ( *end ) );

	/* Allocate device path and space for URI buffer */
	path = zalloc ( len + uri_len + 1 /* NUL */ );
	if ( ! path )
		return NULL;
	uri_buf = ( ( ( void * ) path ) + len );

	/* Construct device path */
	memcpy ( path, parent, prefix_len );
	vendor = ( ( ( void * ) path ) + prefix_len );
	vendor->vendor.Header.Type = HARDWARE_DEVICE_PATH;
	vendor->vendor.Header.SubType = HW_VENDOR_DP;
	vendor->vendor.Header.Length[0] = ( vendor_len & 0xff );
	vendor->vendor.Header.Length[1] = ( vendor_len >> 8 );
	memcpy ( &vendor->vendor.Guid, &ipxe_block_device_path_guid,
		 sizeof ( vendor->vendor.Guid ) );
	format_uri ( uri, uri_buf, ( uri_len + 1 /* NUL */ ) );
	efi_snprintf ( vendor->uri, ( uri_len + 1 /* NUL */ ), "%s", uri_buf );
	end = ( ( ( void * ) vendor ) + vendor_len );
	end->Type = END_DEVICE_PATH_TYPE;
	end->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
	end->Length[0] = sizeof ( *end );

	return path;
}

/**
 * Configure EFI block device as a CD-ROM, if applicable
 *
 * @v block		Block device
 * @v scratch		Scratch data area
 * @ret rc		Return status code
 *
 * The EDK2 code will ignore CD-ROM devices with a block size other
 * than 2048.  While we could require the user to configure the block
 * size appropriately, this is non-trivial and would impose a
 * substantial learning effort on the user.  Instead, we perform
 * essentially the same auto-detection as under a BIOS SAN boot; if
 * the ISO9660 primary volume descriptor is present then we force a
 * block size of 2048 and map read/write requests appropriately.
 */
static int efi_block_parse_iso9660 ( struct efi_block *block, void *scratch ) {
	static const struct iso9660_primary_descriptor_fixed primary_check = {
		.type = ISO9660_TYPE_PRIMARY,
		.id = ISO9660_ID,
	};
	struct iso9660_primary_descriptor *primary = scratch;
	struct efi_block_command_context context;
	unsigned int blksize;
	unsigned int blksize_shift;
	int rc;

	/* Calculate required blocksize shift for potential CD-ROM access */
	blksize = block->capacity.blksize;
	blksize_shift = 0;
	while ( blksize < ISO9660_BLKSIZE ) {
		blksize <<= 1;
		blksize_shift++;
	}
	if ( blksize > ISO9660_BLKSIZE ) {
		/* Cannot be a CD-ROM */
		return 0;
	}

	/* Read primary volume descriptor */
	memset ( &context, 0, sizeof ( context ) );
	context.lba = ( ISO9660_PRIMARY_LBA << blksize_shift );
	context.data = primary;
	context.len = ISO9660_BLKSIZE;
	context.block_rw = block_read;
	if ( ( rc = efi_block_command ( block, efi_block_cmd_rw,
					&context ) ) != 0 ) {
		DBGC ( block, "EFIBLK %#02x could not read ISO9660 primary "
		       "volume descriptor: %s\n",
		       block->drive, strerror ( rc ) );
		return rc;
	}

	/* Do nothing unless this is an ISO image */
	if ( memcmp ( primary, &primary_check, sizeof ( primary_check ) ) != 0 )
		return 0;
	DBGC ( block, "EFIBLK %#02x contains an ISO9660 filesystem; treating "
	       "as CD-ROM\n", block->drive );
	block->blksize_shift = blksize_shift;

	return 0;
}

/**
 * Determing EFI block device capacity and block size
 *
 * @v block		Block device
 * @ret rc		Return status code
 */
static int efi_block_capacity ( struct efi_block *block ) {
	size_t scratch_len;
	void *scratch;
	int rc;

	/* Read read block capacity */
	if ( ( rc = efi_block_command ( block, efi_block_cmd_read_capacity,
					NULL ) ) != 0 )
		goto err_read_capacity;
	block->blksize_shift = 0;

	/* Allocate scratch area */
	scratch_len = ( block->capacity.blksize );
	if ( scratch_len < ISO9660_BLKSIZE )
		scratch_len = ISO9660_BLKSIZE;
	scratch = malloc ( scratch_len );
	if ( ! scratch ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Configure as a CD-ROM, if applicable */
	if ( ( rc = efi_block_parse_iso9660 ( block, scratch ) ) != 0 )
		goto err_parse_iso9660;

	/* Update media descriptor */
	block->media.BlockSize =
		( block->capacity.blksize << block->blksize_shift );
	block->media.LastBlock =
		( ( block->capacity.blocks >> block->blksize_shift ) - 1 );

	/* Success */
	rc = 0;

 err_parse_iso9660:
	free ( scratch );
 err_alloc:
 err_read_capacity:
	return rc;
}

/**
 * Connect all possible drivers to EFI block device
 *
 * @v block		Block device
 */
static void efi_block_connect ( struct efi_block *block ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_STATUS efirc;
	int rc;

	/* Try to connect all possible drivers to this block device */
	if ( ( efirc = bs->ConnectController ( block->handle, NULL,
					       NULL, 1 ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( block, "EFIBLK %#02x could not connect drivers: %s\n",
		       block->drive, strerror ( rc ) );
		/* May not be an error; may already be connected */
	}
	DBGC2 ( block, "EFIBLK %#02x supports protocols:\n", block->drive );
	DBGC2_EFI_PROTOCOLS ( block, block->handle );
}

/**
 * Hook EFI block device
 *
 * @v uri		URI
 * @v drive		Drive number
 * @ret drive		Drive number, or negative error
 */
static int efi_block_hook ( struct uri *uri, unsigned int drive ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct efi_snp_device *snpdev;
	struct efi_block *block;
	EFI_STATUS efirc;
	int rc;

	/* Check that drive number is not already in use */
	if ( efi_block_find ( drive ) ) {
		rc = -EADDRINUSE;
		goto err_in_use;
	}

	/* Allocate and initialise structure */
	block = zalloc ( sizeof ( *block ) );
	if ( ! block ) {
		rc = -ENOMEM;
		goto err_zalloc;
	}
	ref_init ( &block->refcnt, efi_block_free );
	intf_init ( &block->intf, &efi_block_desc, &block->refcnt );
	block->uri = uri_get ( uri );
	block->drive = drive;
	block->block_rc = -EINPROGRESS;
	block->media.MediaPresent = 1;
	block->media.LogicalBlocksPerPhysicalBlock = 1;
	block->block_io.Revision = EFI_BLOCK_IO_PROTOCOL_REVISION3;
	block->block_io.Media = &block->media;
	block->block_io.Reset = efi_block_io_reset;
	block->block_io.ReadBlocks = efi_block_io_read;
	block->block_io.WriteBlocks = efi_block_io_write;
	block->block_io.FlushBlocks = efi_block_io_flush;
	intf_init ( &block->command, &efi_block_cmd_desc, &block->refcnt );
	timer_init ( &block->timer, efi_block_cmd_expired, &block->refcnt );

	/* Find an appropriate parent device handle */
	snpdev = last_opened_snpdev();
	if ( ! snpdev ) {
		DBGC ( block, "EFIBLK %#02x could not identify SNP device\n",
		       block->drive );
		rc = -ENODEV;
		goto err_no_snpdev;
	}

	/* Construct device path */
	block->path = efi_block_path ( block->uri, snpdev->path );
	if ( ! block->path ) {
		rc = -ENOMEM;
		goto err_path;
	}
	DBGC ( block, "EFIBLK %#02x has device path %s\n",
	       block->drive, efi_devpath_text ( block->path ) );

	/* Add to list of block devices */
	list_add ( &block->list, &efi_block_devices );

	/* Open block device interface */
	if ( ( rc = efi_block_reopen ( block ) ) != 0 )
		goto err_reopen;

	/* Determine capacity and block size */
	if ( ( rc = efi_block_capacity ( block ) ) != 0 )
		goto err_capacity;

	/* Install protocols */
	if ( ( efirc = bs->InstallMultipleProtocolInterfaces (
			&block->handle,
			&efi_block_io_protocol_guid, &block->block_io,
			&efi_device_path_protocol_guid, block->path,
			NULL ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( block, "EFIBLK %#02x could not install protocols: %s\n",
		       block->drive, strerror ( rc ) );
		goto err_install;
	}

	/* Connect all possible protocols */
	efi_block_connect ( block );

	return drive;

	bs->UninstallMultipleProtocolInterfaces (
			block->handle,
			&efi_block_io_protocol_guid, &block->block_io,
			&efi_device_path_protocol_guid, block->path, NULL );
 err_install:
 err_capacity:
	efi_block_restart ( block, rc );
	intf_shutdown ( &block->intf, rc );
 err_reopen:
	list_del ( &block->list );
 err_no_snpdev:
 err_path:
	ref_put ( &block->refcnt );
 err_zalloc:
 err_in_use:
	return rc;
}

/**
 * Unhook EFI block device
 *
 * @v drive		Drive number
 */
static void efi_block_unhook ( unsigned int drive ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct efi_block *block;

	/* Find block device */
	block = efi_block_find ( drive );
	if ( ! block ) {
		DBG ( "EFIBLK cannot find drive %#02x\n", drive );
		return;
	}

	/* Uninstall protocols */
	bs->UninstallMultipleProtocolInterfaces (
			block->handle,
			&efi_block_io_protocol_guid, &block->block_io,
			&efi_device_path_protocol_guid, block->path, NULL );

	/* Close any outstanding commands and shut down interface */
	efi_block_restart ( block, 0 );
	intf_shutdown ( &block->intf, 0 );

	/* Remove from list of block devices */
	list_del ( &block->list );

	/* Drop list's reference to drive */
	ref_put ( &block->refcnt );
}

/**
 * Describe EFI block device
 *
 * @v drive		Drive number
 * @ret rc		Return status code
 */
static int efi_block_describe ( unsigned int drive ) {
	struct efi_block *block;

	/* Find block device */
	block = efi_block_find ( drive );
	if ( ! block ) {
		DBG ( "EFIBLK cannot find drive %#02x\n", drive );
		return -ENODEV;
	}

	return 0;
}

/**
 * Try booting from child device of EFI block device
 *
 * @v block		Block device
 * @v handle		EFI handle
 * @ret rc		Return status code
 */
static int efi_block_boot_image ( struct efi_block *block,
				  EFI_HANDLE handle, EFI_HANDLE *image ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
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
		DBGC ( block, "EFIBLK %#02x found filesystem with no device "
		       "path??", block->drive );
		rc = -EEFI ( efirc );
		goto err_open_device_path;
	}

	/* Check if this device is a child of our block device */
	end = efi_devpath_end ( block->path );
	prefix_len = ( ( ( void * ) end ) - ( ( void * ) block->path ) );
	if ( memcmp ( path.path, block->path, prefix_len ) != 0 ) {
		/* Not a child device */
		rc = -ENOTTY;
		goto err_not_child;
	}
	DBGC ( block, "EFIBLK %#02x found child device %s\n",
	       block->drive, efi_devpath_text ( path.path ) );

	/* Construct device path for boot image */
	end = efi_devpath_end ( path.path );
	prefix_len = ( ( ( void * ) end ) - ( ( void * ) path.path ) );
	filepath_len = ( SIZE_OF_FILEPATH_DEVICE_PATH +
			 sizeof ( efi_block_boot_filename ) );
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
	memcpy ( filepath->PathName, efi_block_boot_filename,
		 sizeof ( efi_block_boot_filename ) );
	end = ( ( ( void * ) filepath ) + filepath_len );
	end->Type = END_DEVICE_PATH_TYPE;
	end->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
	end->Length[0] = sizeof ( *end );
	DBGC ( block, "EFIBLK %#02x trying to load %s\n",
	       block->drive, efi_devpath_text ( boot_path ) );

	/* Try loading boot image from this device */
	if ( ( efirc = bs->LoadImage ( FALSE, efi_image_handle, boot_path,
				       NULL, 0, image ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( block, "EFIBLK %#02x could not load image: %s\n",
		       block->drive, strerror ( rc ) );
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
 * @ret rc		Return status code
 */
static int efi_block_boot ( unsigned int drive ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	struct efi_block *block;
	EFI_HANDLE *handles;
	EFI_HANDLE image = NULL;
	UINTN count;
	unsigned int i;
	EFI_STATUS efirc;
	int rc;

	/* Find block device */
	block = efi_block_find ( drive );
	if ( ! block ) {
		DBG ( "EFIBLK cannot find drive %#02x\n", drive );
		rc = -ENODEV;
		goto err_block_find;
	}

	/* Connect all possible protocols */
	efi_block_connect ( block );

	/* Locate all handles supporting the Simple File System protocol */
	if ( ( efirc = bs->LocateHandleBuffer (
			ByProtocol, &efi_simple_file_system_protocol_guid,
			NULL, &count, &handles ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( block, "EFIBLK %#02x cannot locate file systems: %s\n",
		       block->drive, strerror ( rc ) );
		goto err_locate_file_systems;
	}

	/* Try booting from any available child device containing a
	 * suitable boot image.  This is something of a wild stab in
	 * the dark, but should end up conforming to user expectations
	 * most of the time.
	 */
	rc = -ENOENT;
	for ( i = 0 ; i < count ; i++ ) {
		if ( ( rc = efi_block_boot_image ( block, handles[i],
						   &image ) ) != 0 )
			continue;
		DBGC ( block, "EFIBLK %#02x found boot image\n", block->drive );
		efirc = bs->StartImage ( image, NULL, NULL );
		rc = ( efirc ? -EEFI ( efirc ) : 0 );
		bs->UnloadImage ( image );
		DBGC ( block, "EFIBLK %#02x boot image returned: %s\n",
		       block->drive, strerror ( rc ) );
		break;
	}

	bs->FreePool ( handles );
 err_locate_file_systems:
 err_block_find:
	return rc;
}

PROVIDE_SANBOOT_INLINE ( efi, san_default_drive );
PROVIDE_SANBOOT ( efi, san_hook, efi_block_hook );
PROVIDE_SANBOOT ( efi, san_unhook, efi_block_unhook );
PROVIDE_SANBOOT ( efi, san_describe, efi_block_describe );
PROVIDE_SANBOOT ( efi, san_boot, efi_block_boot );
