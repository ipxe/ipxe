/*
 * Copyright (C) 2017 Michael Brown <mbrown@fensystems.co.uk>.
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
 * SAN booting
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <ipxe/xfer.h>
#include <ipxe/open.h>
#include <ipxe/timer.h>
#include <ipxe/process.h>
#include <ipxe/iso9660.h>
#include <ipxe/dhcp.h>
#include <ipxe/settings.h>
#include <ipxe/sanboot.h>

/**
 * Default SAN drive number
 *
 * The drive number is a meaningful concept only in a BIOS
 * environment, where it represents the INT13 drive number (0x80 for
 * the first hard disk).  We retain it in other environments to allow
 * for a simple way for iPXE commands to refer to SAN drives.
 */
#define SAN_DEFAULT_DRIVE 0x80

/**
 * Timeout for block device commands (in ticks)
 *
 * Underlying devices should ideally never become totally stuck.
 * However, if they do, then the blocking SAN APIs provide no means
 * for the caller to cancel the operation, and the machine appears to
 * hang.  Use an overall timeout for all commands to avoid this
 * problem and bounce timeout failures to the caller.
 */
#define SAN_COMMAND_TIMEOUT ( 15 * TICKS_PER_SEC )

/** List of SAN devices */
LIST_HEAD ( san_devices );

/**
 * Find SAN device by drive number
 *
 * @v drive		Drive number
 * @ret sandev		SAN device, or NULL
 */
struct san_device * sandev_find ( unsigned int drive ) {
	struct san_device *sandev;

	list_for_each_entry ( sandev, &san_devices, list ) {
		if ( sandev->drive == drive )
			return sandev;
	}
	return NULL;
}

/**
 * Free SAN device
 *
 * @v refcnt		Reference count
 */
static void sandev_free ( struct refcnt *refcnt ) {
	struct san_device *sandev =
		container_of ( refcnt, struct san_device, refcnt );

	assert ( ! timer_running ( &sandev->timer ) );
	uri_put ( sandev->uri );
	free ( sandev );
}

/**
 * Close SAN device command
 *
 * @v sandev		SAN device
 * @v rc		Reason for close
 */
static void sandev_command_close ( struct san_device *sandev, int rc ) {

	/* Stop timer */
	stop_timer ( &sandev->timer );

	/* Restart interface */
	intf_restart ( &sandev->command, rc );

	/* Record command status */
	sandev->command_rc = rc;
}

/**
 * Record SAN device capacity
 *
 * @v sandev		SAN device
 * @v capacity		SAN device capacity
 */
static void sandev_command_capacity ( struct san_device *sandev,
				      struct block_device_capacity *capacity ) {

	/* Record raw capacity information */
	memcpy ( &sandev->capacity, capacity, sizeof ( sandev->capacity ) );
}

/** SAN device command interface operations */
static struct interface_operation sandev_command_op[] = {
	INTF_OP ( intf_close, struct san_device *, sandev_command_close ),
	INTF_OP ( block_capacity, struct san_device *,
		  sandev_command_capacity ),
};

/** SAN device command interface descriptor */
static struct interface_descriptor sandev_command_desc =
	INTF_DESC ( struct san_device, command, sandev_command_op );

/**
 * Handle SAN device command timeout
 *
 * @v retry		Retry timer
 */
static void sandev_command_expired ( struct retry_timer *timer,
				     int over __unused ) {
	struct san_device *sandev =
		container_of ( timer, struct san_device, timer );

	sandev_command_close ( sandev, -ETIMEDOUT );
}

/**
 * Restart SAN device interface
 *
 * @v sandev		SAN device
 * @v rc		Reason for restart
 */
static void sandev_restart ( struct san_device *sandev, int rc ) {

	/* Restart block device interface */
	intf_nullify ( &sandev->command ); /* avoid potential loops */
	intf_restart ( &sandev->block, rc );

	/* Close any outstanding command */
	sandev_command_close ( sandev, rc );

	/* Record device error */
	sandev->block_rc = rc;
}

/**
 * (Re)open SAN device
 *
 * @v sandev		SAN device
 * @ret rc		Return status code
 *
 * This function will block until the device is available.
 */
int sandev_reopen ( struct san_device *sandev ) {
	int rc;

	/* Close any outstanding command and restart interface */
	sandev_restart ( sandev, -ECONNRESET );

	/* Mark device as being not yet open */
	sandev->block_rc = -EINPROGRESS;

	/* Open block device interface */
	if ( ( rc = xfer_open_uri ( &sandev->block, sandev->uri ) ) != 0 ) {
		DBGC ( sandev, "SAN %#02x could not (re)open URI: %s\n",
		       sandev->drive, strerror ( rc ) );
		return rc;
	}

	/* Wait for device to become available */
	while ( sandev->block_rc == -EINPROGRESS ) {
		step();
		if ( xfer_window ( &sandev->block ) != 0 ) {
			sandev->block_rc = 0;
			return 0;
		}
	}

	DBGC ( sandev, "SAN %#02x never became available: %s\n",
	       sandev->drive, strerror ( sandev->block_rc ) );
	return sandev->block_rc;
}

/**
 * Handle closure of underlying block device interface
 *
 * @v sandev		SAN device
 * @ret rc		Reason for close
 */
static void sandev_block_close ( struct san_device *sandev, int rc ) {

	/* Any closure is an error from our point of view */
	if ( rc == 0 )
		rc = -ENOTCONN;
	DBGC ( sandev, "SAN %#02x went away: %s\n",
	       sandev->drive, strerror ( rc ) );

	/* Close any outstanding command and restart interface */
	sandev_restart ( sandev, rc );
}

/**
 * Check SAN device flow control window
 *
 * @v sandev		SAN device
 */
static size_t sandev_block_window ( struct san_device *sandev __unused ) {

	/* We are never ready to receive data via this interface.
	 * This prevents objects that support both block and stream
	 * interfaces from attempting to send us stream data.
	 */
	return 0;
}

/** SAN device block interface operations */
static struct interface_operation sandev_block_op[] = {
	INTF_OP ( intf_close, struct san_device *, sandev_block_close ),
	INTF_OP ( xfer_window, struct san_device *, sandev_block_window ),
};

/** SAN device block interface descriptor */
static struct interface_descriptor sandev_block_desc =
	INTF_DESC ( struct san_device, block, sandev_block_op );

/** SAN device read/write command parameters */
struct san_command_rw_params {
	/** SAN device read/write operation */
	int ( * block_rw ) ( struct interface *control, struct interface *data,
			     uint64_t lba, unsigned int count,
			     userptr_t buffer, size_t len );
	/** Data buffer */
	userptr_t buffer;
	/** Starting LBA */
	uint64_t lba;
	/** Block count */
	unsigned int count;
};

/** SAN device command parameters */
union san_command_params {
	/** Read/write command parameters */
	struct san_command_rw_params rw;
};

/**
 * Initiate SAN device read/write command
 *
 * @v sandev		SAN device
 * @v params		Command parameters
 * @ret rc		Return status code
 */
static int sandev_command_rw ( struct san_device *sandev,
			       const union san_command_params *params ) {
	size_t len = ( params->rw.count * sandev->capacity.blksize );
	int rc;

	/* Initiate read/write command */
	if ( ( rc = params->rw.block_rw ( &sandev->block, &sandev->command,
					  params->rw.lba, params->rw.count,
					  params->rw.buffer, len ) ) != 0 ) {
		DBGC ( sandev, "SAN %#02x could not initiate read/write: "
		       "%s\n", sandev->drive, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Initiate SAN device read capacity command
 *
 * @v sandev		SAN device
 * @v params		Command parameters
 * @ret rc		Return status code
 */
static int
sandev_command_read_capacity ( struct san_device *sandev,
			       const union san_command_params *params __unused){
	int rc;

	/* Initiate read capacity command */
	if ( ( rc = block_read_capacity ( &sandev->block,
					  &sandev->command ) ) != 0 ) {
		DBGC ( sandev, "SAN %#02x could not initiate read capacity: "
		       "%s\n", sandev->drive, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Execute a single SAN device command and wait for completion
 *
 * @v sandev		SAN device
 * @v command		Command
 * @v params		Command parameters (if required)
 * @ret rc		Return status code
 */
static int
sandev_command ( struct san_device *sandev,
		 int ( * command ) ( struct san_device *sandev,
				     const union san_command_params *params ),
		 const union san_command_params *params ) {
	int rc;

	/* Sanity check */
	assert ( ! timer_running ( &sandev->timer ) );

	/* Reopen block device if applicable */
	if ( sandev_needs_reopen ( sandev ) &&
	     ( ( rc = sandev_reopen ( sandev ) ) != 0 ) ) {
	     goto err_reopen;
	}

	/* Start expiry timer */
	start_timer_fixed ( &sandev->timer, SAN_COMMAND_TIMEOUT );

	/* Initiate command */
	if ( ( rc = command ( sandev, params ) ) != 0 )
		goto err_op;

	/* Wait for command to complete */
	while ( timer_running ( &sandev->timer ) )
		step();

	/* Collect return status */
	rc = sandev->command_rc;

	return rc;

 err_op:
	stop_timer ( &sandev->timer );
 err_reopen:
	return rc;
}

/**
 * Reset SAN device
 *
 * @v sandev		SAN device
 * @ret rc		Return status code
 */
int sandev_reset ( struct san_device *sandev ) {
	int rc;

	DBGC ( sandev, "SAN %#02x reset\n", sandev->drive );

	/* Close and reopen underlying block device */
	if ( ( rc = sandev_reopen ( sandev ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Read from or write to SAN device
 *
 * @v sandev		SAN device
 * @v lba		Starting logical block address
 * @v count		Number of logical blocks
 * @v buffer		Data buffer
 * @v block_rw		Block read/write method
 * @ret rc		Return status code
 */
int sandev_rw ( struct san_device *sandev, uint64_t lba,
		unsigned int count, userptr_t buffer,
		int ( * block_rw ) ( struct interface *control,
				     struct interface *data,
				     uint64_t lba, unsigned int count,
				     userptr_t buffer, size_t len ) ) {
	union san_command_params params;
	unsigned int remaining;
	size_t frag_len;
	int rc;

	/* Initialise command parameters */
	params.rw.block_rw = block_rw;
	params.rw.buffer = buffer;
	params.rw.lba = ( lba << sandev->blksize_shift );
	params.rw.count = sandev->capacity.max_count;
	remaining = ( count << sandev->blksize_shift );

	/* Read/write fragments */
	while ( remaining ) {

		/* Determine fragment length */
		if ( params.rw.count > remaining )
			params.rw.count = remaining;

		/* Execute command */
		if ( ( rc = sandev_command ( sandev, sandev_command_rw,
					     &params ) ) != 0 )
			return rc;

		/* Move to next fragment */
		frag_len = ( sandev->capacity.blksize * params.rw.count );
		params.rw.buffer = userptr_add ( params.rw.buffer, frag_len );
		params.rw.lba += params.rw.count;
		remaining -= params.rw.count;
	}

	return 0;
}

/**
 * Configure SAN device as a CD-ROM, if applicable
 *
 * @v sandev		SAN device
 * @ret rc		Return status code
 *
 * Both BIOS and UEFI require SAN devices to be accessed with a block
 * size of 2048.  While we could require the user to configure the
 * block size appropriately, this is non-trivial and would impose a
 * substantial learning effort on the user.  Instead, we check for the
 * presence of the ISO9660 primary volume descriptor and, if found,
 * then we force a block size of 2048 and map read/write requests
 * appropriately.
 */
static int sandev_parse_iso9660 ( struct san_device *sandev ) {
	static const struct iso9660_primary_descriptor_fixed primary_check = {
		.type = ISO9660_TYPE_PRIMARY,
		.id = ISO9660_ID,
	};
	struct iso9660_primary_descriptor *primary;
	unsigned int blksize;
	unsigned int blksize_shift;
	unsigned int lba;
	unsigned int count;
	int rc;

	/* Calculate required blocksize shift for potential CD-ROM access */
	blksize = sandev->capacity.blksize;
	blksize_shift = 0;
	while ( blksize < ISO9660_BLKSIZE ) {
		blksize <<= 1;
		blksize_shift++;
	}
	if ( blksize > ISO9660_BLKSIZE ) {
		/* Cannot be a CD-ROM.  This is not an error. */
		rc = 0;
		goto invalid_blksize;
	}
	lba = ( ISO9660_PRIMARY_LBA << blksize_shift );
	count = ( 1 << blksize_shift );

	/* Allocate scratch area */
	primary = malloc ( ISO9660_BLKSIZE );
	if ( ! primary ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Read primary volume descriptor */
	if ( ( rc = sandev_rw ( sandev, lba, count, virt_to_user ( primary ),
				block_read ) ) != 0 ) {
		DBGC ( sandev, "SAN %#02x could not read ISO9660 primary"
		       "volume descriptor: %s\n",
		       sandev->drive, strerror ( rc ) );
		goto err_rw;
	}

	/* Configure as CD-ROM if applicable */
	if ( memcmp ( primary, &primary_check, sizeof ( primary_check ) ) == 0){
		DBGC ( sandev, "SAN %#02x contains an ISO9660 filesystem; "
		       "treating as CD-ROM\n", sandev->drive );
		sandev->blksize_shift = blksize_shift;
		sandev->is_cdrom = 1;
	}

 err_rw:
	free ( primary );
 err_alloc:
 invalid_blksize:
	return rc;
}

/**
 * Allocate SAN device
 *
 * @ret sandev		SAN device, or NULL
 */
struct san_device * alloc_sandev ( struct uri *uri, size_t priv_size ) {
	struct san_device *sandev;

	/* Allocate and initialise structure */
	sandev = zalloc ( sizeof ( *sandev ) + priv_size );
	if ( ! sandev )
		return NULL;
	ref_init ( &sandev->refcnt, sandev_free );
	sandev->uri = uri_get ( uri );
	intf_init ( &sandev->block, &sandev_block_desc, &sandev->refcnt );
	sandev->block_rc = -EINPROGRESS;
	intf_init ( &sandev->command, &sandev_command_desc, &sandev->refcnt );
	timer_init ( &sandev->timer, sandev_command_expired, &sandev->refcnt );
	sandev->priv = ( ( ( void * ) sandev ) + sizeof ( *sandev ) );

	return sandev;
}

/**
 * Register SAN device
 *
 * @v sandev		SAN device
 * @ret rc		Return status code
 */
int register_sandev ( struct san_device *sandev ) {
	int rc;

	/* Check that drive number is not in use */
	if ( sandev_find ( sandev->drive ) != NULL ) {
		DBGC ( sandev, "SAN %#02x is already in use\n", sandev->drive );
		return -EADDRINUSE;
	}

	/* Read device capacity */
	if ( ( rc = sandev_command ( sandev, sandev_command_read_capacity,
				     NULL ) ) != 0 )
		return rc;

	/* Configure as a CD-ROM, if applicable */
	if ( ( rc = sandev_parse_iso9660 ( sandev ) ) != 0 )
		return rc;

	/* Add to list of SAN devices */
	list_add_tail ( &sandev->list, &san_devices );
	DBGC ( sandev, "SAN %#02x registered\n", sandev->drive );

	return 0;
}

/**
 * Unregister SAN device
 *
 * @v sandev		SAN device
 */
void unregister_sandev ( struct san_device *sandev ) {

	/* Sanity check */
	assert ( ! timer_running ( &sandev->timer ) );

	/* Shut down interfaces */
	intfs_shutdown ( 0, &sandev->block, &sandev->command, NULL );

	/* Remove from list of SAN devices */
	list_del ( &sandev->list );
	DBGC ( sandev, "SAN %#02x unregistered\n", sandev->drive );
}

/** The "san-drive" setting */
const struct setting san_drive_setting __setting ( SETTING_SANBOOT_EXTRA,
						   san-drive ) = {
	.name = "san-drive",
	.description = "SAN drive number",
	.tag = DHCP_EB_SAN_DRIVE,
	.type = &setting_type_uint8,
};

/**
 * Get default SAN drive number
 *
 * @ret drive		Default drive number
 */
unsigned int san_default_drive ( void ) {
	unsigned long drive;

	/* Use "san-drive" setting, if specified */
	if ( fetch_uint_setting ( NULL, &san_drive_setting, &drive ) >= 0 )
		return drive;

	/* Otherwise, default to booting from first hard disk */
	return SAN_DEFAULT_DRIVE;
}
