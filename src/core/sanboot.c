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
#include <ipxe/quiesce.h>
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

/**
 * Default number of times to retry commands
 *
 * We may need to retry commands.  For example, the underlying
 * connection may be closed by the SAN target due to an inactivity
 * timeout, or the SAN target may return pointless "error" messages
 * such as "SCSI power-on occurred".
 */
#define SAN_DEFAULT_RETRIES 10

/**
 * Delay between reopening attempts
 *
 * Some SAN targets will always accept connections instantly and
 * report a temporary unavailability by e.g. failing the TEST UNIT
 * READY command.  Avoid bombarding such targets by introducing a
 * small delay between attempts.
 */
#define SAN_REOPEN_DELAY_SECS 5

/** List of SAN devices */
LIST_HEAD ( san_devices );

/** Number of times to retry commands */
static unsigned long san_retries = SAN_DEFAULT_RETRIES;

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
	unsigned int i;

	assert ( ! timer_running ( &sandev->timer ) );
	assert ( ! sandev->active );
	assert ( list_empty ( &sandev->opened ) );
	for ( i = 0 ; i < sandev->paths ; i++ ) {
		uri_put ( sandev->path[i].uri );
		assert ( sandev->path[i].desc == NULL );
	}
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
 * Open SAN path
 *
 * @v sanpath		SAN path
 * @ret rc		Return status code
 */
static int sanpath_open ( struct san_path *sanpath ) {
	struct san_device *sandev = sanpath->sandev;
	int rc;

	/* Sanity check */
	list_check_contains_entry ( sanpath, &sandev->closed, list );

	/* Open interface */
	if ( ( rc = xfer_open_uri ( &sanpath->block, sanpath->uri ) ) != 0 ) {
		DBGC ( sandev, "SAN %#02x.%d could not (re)open URI: "
		       "%s\n", sandev->drive, sanpath->index, strerror ( rc ) );
		return rc;
	}

	/* Update ACPI descriptor, if applicable */
	if ( ! ( sandev->flags & SAN_NO_DESCRIBE ) ) {
		if ( sanpath->desc )
			acpi_del ( sanpath->desc );
		sanpath->desc = acpi_describe ( &sanpath->block );
		if ( sanpath->desc )
			acpi_add ( sanpath->desc );
	}

	/* Start process */
	process_add ( &sanpath->process );

	/* Mark as opened */
	list_del ( &sanpath->list );
	list_add_tail ( &sanpath->list, &sandev->opened );

	/* Record as in progress */
	sanpath->path_rc = -EINPROGRESS;

	return 0;
}

/**
 * Close SAN path
 *
 * @v sanpath		SAN path
 * @v rc		Reason for close
 */
static void sanpath_close ( struct san_path *sanpath, int rc ) {
	struct san_device *sandev = sanpath->sandev;

	/* Record status */
	sanpath->path_rc = rc;

	/* Mark as closed */
	list_del ( &sanpath->list );
	list_add_tail ( &sanpath->list, &sandev->closed );

	/* Stop process */
	process_del ( &sanpath->process );

	/* Restart interfaces, avoiding potential loops */
	if ( sanpath == sandev->active ) {
		intfs_restart ( rc, &sandev->command, &sanpath->block, NULL );
		sandev->active = NULL;
		sandev_command_close ( sandev, rc );
	} else {
		intf_restart ( &sanpath->block, rc );
	}
}

/**
 * Handle closure of underlying block device interface
 *
 * @v sanpath		SAN path
 * @v rc		Reason for close
 */
static void sanpath_block_close ( struct san_path *sanpath, int rc ) {
	struct san_device *sandev = sanpath->sandev;

	/* Any closure is an error from our point of view */
	if ( rc == 0 )
		rc = -ENOTCONN;
	DBGC ( sandev, "SAN %#02x.%d closed: %s\n",
	       sandev->drive, sanpath->index, strerror ( rc ) );

	/* Close path */
	sanpath_close ( sanpath, rc );
}

/**
 * Check flow control window
 *
 * @v sanpath		SAN path
 */
static size_t sanpath_block_window ( struct san_path *sanpath __unused ) {

	/* We are never ready to receive data via this interface.
	 * This prevents objects that support both block and stream
	 * interfaces from attempting to send us stream data.
	 */
	return 0;
}

/**
 * SAN path process
 *
 * @v sanpath		SAN path
 */
static void sanpath_step ( struct san_path *sanpath ) {
	struct san_device *sandev = sanpath->sandev;

	/* Ignore if we are already the active device */
	if ( sanpath == sandev->active )
		return;

	/* Wait until path has become available */
	if ( ! xfer_window ( &sanpath->block ) )
		return;

	/* Record status */
	sanpath->path_rc = 0;

	/* Mark as active path or close as applicable */
	if ( ! sandev->active ) {
		DBGC ( sandev, "SAN %#02x.%d is active\n",
		       sandev->drive, sanpath->index );
		sandev->active = sanpath;
	} else {
		DBGC ( sandev, "SAN %#02x.%d is available\n",
		       sandev->drive, sanpath->index );
		sanpath_close ( sanpath, 0 );
	}
}

/** SAN path block interface operations */
static struct interface_operation sanpath_block_op[] = {
	INTF_OP ( intf_close, struct san_path *, sanpath_block_close ),
	INTF_OP ( xfer_window, struct san_path *, sanpath_block_window ),
	INTF_OP ( xfer_window_changed, struct san_path *, sanpath_step ),
};

/** SAN path block interface descriptor */
static struct interface_descriptor sanpath_block_desc =
	INTF_DESC ( struct san_path, block, sanpath_block_op );

/** SAN path process descriptor */
static struct process_descriptor sanpath_process_desc =
	PROC_DESC_ONCE ( struct san_path, process, sanpath_step );

/**
 * Restart SAN device interface
 *
 * @v sandev		SAN device
 * @v rc		Reason for restart
 */
static void sandev_restart ( struct san_device *sandev, int rc ) {
	struct san_path *sanpath;

	/* Restart all block device interfaces */
	while ( ( sanpath = list_first_entry ( &sandev->opened,
					       struct san_path, list ) ) ) {
		sanpath_close ( sanpath, rc );
	}

	/* Clear active path */
	sandev->active = NULL;

	/* Close any outstanding command */
	sandev_command_close ( sandev, rc );
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
	struct san_path *sanpath;
	int rc;

	/* Unquiesce system */
	unquiesce();

	/* Close any outstanding command and restart interfaces */
	sandev_restart ( sandev, -ECONNRESET );
	assert ( sandev->active == NULL );
	assert ( list_empty ( &sandev->opened ) );

	/* Open all paths */
	while ( ( sanpath = list_first_entry ( &sandev->closed,
					       struct san_path, list ) ) ) {
		if ( ( rc = sanpath_open ( sanpath ) ) != 0 )
			goto err_open;
	}

	/* Wait for any device to become available, or for all devices
	 * to fail.
	 */
	while ( sandev->active == NULL ) {
		step();
		if ( list_empty ( &sandev->opened ) ) {
			/* Get status of the first device to be
			 * closed.  Do this on the basis that earlier
			 * errors (e.g. "invalid IQN") are probably
			 * more interesting than later errors
			 * (e.g. "TCP timeout").
			 */
			rc = -ENODEV;
			list_for_each_entry ( sanpath, &sandev->closed, list ) {
				rc = sanpath->path_rc;
				break;
			}
			DBGC ( sandev, "SAN %#02x never became available: %s\n",
			       sandev->drive, strerror ( rc ) );
			goto err_none;
		}
	}

	assert ( ! list_empty ( &sandev->opened ) );
	return 0;

 err_none:
 err_open:
	sandev_restart ( sandev, rc );
	return rc;
}

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
	struct san_path *sanpath = sandev->active;
	size_t len = ( params->rw.count * sandev->capacity.blksize );
	int rc;

	/* Sanity check */
	assert ( sanpath != NULL );

	/* Initiate read/write command */
	if ( ( rc = params->rw.block_rw ( &sanpath->block, &sandev->command,
					  params->rw.lba, params->rw.count,
					  params->rw.buffer, len ) ) != 0 ) {
		DBGC ( sandev, "SAN %#02x.%d could not initiate read/write: "
		       "%s\n", sandev->drive, sanpath->index, strerror ( rc ) );
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
	struct san_path *sanpath = sandev->active;
	int rc;

	/* Sanity check */
	assert ( sanpath != NULL );

	/* Initiate read capacity command */
	if ( ( rc = block_read_capacity ( &sanpath->block,
					  &sandev->command ) ) != 0 ) {
		DBGC ( sandev, "SAN %#02x.%d could not initiate read capacity: "
		       "%s\n", sandev->drive, sanpath->index, strerror ( rc ) );
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
	unsigned int retries = 0;
	int rc;

	/* Sanity check */
	assert ( ! timer_running ( &sandev->timer ) );

	/* Unquiesce system */
	unquiesce();

	/* (Re)try command */
	do {

		/* Reopen block device if applicable */
		if ( sandev_needs_reopen ( sandev ) &&
		     ( ( rc = sandev_reopen ( sandev ) ) != 0 ) ) {

			/* Delay reopening attempts */
			sleep_fixed ( SAN_REOPEN_DELAY_SECS );

			/* Retry opening indefinitely for multipath devices */
			if ( sandev->paths <= 1 )
				retries++;

			continue;
		}

		/* Initiate command */
		if ( ( rc = command ( sandev, params ) ) != 0 ) {
			retries++;
			continue;
		}

		/* Start expiry timer */
		start_timer_fixed ( &sandev->timer, SAN_COMMAND_TIMEOUT );

		/* Wait for command to complete */
		while ( timer_running ( &sandev->timer ) )
			step();

		/* Check command status */
		if ( ( rc = sandev->command_rc ) != 0 ) {
			retries++;
			continue;
		}

		return 0;

	} while ( retries <= san_retries );

	/* Sanity check */
	assert ( ! timer_running ( &sandev->timer ) );

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
static int sandev_rw ( struct san_device *sandev, uint64_t lba,
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
 * Read from SAN device
 *
 * @v sandev		SAN device
 * @v lba		Starting logical block address
 * @v count		Number of logical blocks
 * @v buffer		Data buffer
 * @ret rc		Return status code
 */
int sandev_read ( struct san_device *sandev, uint64_t lba,
		  unsigned int count, userptr_t buffer ) {
	int rc;

	/* Read from device */
	if ( ( rc = sandev_rw ( sandev, lba, count, buffer, block_read ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Write to SAN device
 *
 * @v sandev		SAN device
 * @v lba		Starting logical block address
 * @v count		Number of logical blocks
 * @v buffer		Data buffer
 * @ret rc		Return status code
 */
int sandev_write ( struct san_device *sandev, uint64_t lba,
		   unsigned int count, userptr_t buffer ) {
	int rc;

	/* Write to device */
	if ( ( rc = sandev_rw ( sandev, lba, count, buffer, block_write ) ) != 0 )
		return rc;

	/* Quiesce system.  This is a heuristic designed to ensure
	 * that the system is quiesced before Windows starts up, since
	 * a Windows SAN boot will typically write a status flag to
	 * the disk as its last action before transferring control to
	 * the native drivers.
	 */
	quiesce();

	return 0;
}

/**
 * Describe SAN device
 *
 * @v sandev		SAN device
 * @ret rc		Return status code
 *
 * Allow connections to progress until all existent path descriptors
 * are complete.
 */
static int sandev_describe ( struct san_device *sandev ) {
	struct san_path *sanpath;
	struct acpi_descriptor *desc;
	int rc;

	/* Wait for all paths to be either described or closed */
	while ( 1 ) {

		/* Allow connections to progress */
		step();

		/* Fail if any closed path has an incomplete descriptor */
		list_for_each_entry ( sanpath, &sandev->closed, list ) {
			desc = sanpath->desc;
			if ( ! desc )
				continue;
			if ( ( rc = desc->model->complete ( desc ) ) != 0 ) {
				DBGC ( sandev, "SAN %#02x.%d could not be "
				       "described: %s\n", sandev->drive,
				       sanpath->index, strerror ( rc ) );
				return rc;
			}
		}

		/* Succeed if no paths have an incomplete descriptor */
		rc = 0;
		list_for_each_entry ( sanpath, &sandev->opened, list ) {
			desc = sanpath->desc;
			if ( ! desc )
				continue;
			if ( ( rc = desc->model->complete ( desc ) ) != 0 )
				break;
		}
		if ( rc == 0 )
			return 0;
	}
}

/**
 * Remove SAN device descriptors
 *
 * @v sandev		SAN device
 */
static void sandev_undescribe ( struct san_device *sandev ) {
	struct san_path *sanpath;
	unsigned int i;

	/* Remove all ACPI descriptors */
	for ( i = 0 ; i < sandev->paths ; i++ ) {
		sanpath = &sandev->path[i];
		if ( sanpath->desc ) {
			acpi_del ( sanpath->desc );
			sanpath->desc = NULL;
		}
	}
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
	union {
		struct iso9660_primary_descriptor primary;
		char bytes[ISO9660_BLKSIZE];
	} *scratch;
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
	scratch = malloc ( ISO9660_BLKSIZE );
	if ( ! scratch ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Read primary volume descriptor */
	if ( ( rc = sandev_read ( sandev, lba, count,
				  virt_to_user ( scratch ) ) ) != 0 ) {
		DBGC ( sandev, "SAN %#02x could not read ISO9660 primary"
		       "volume descriptor: %s\n",
		       sandev->drive, strerror ( rc ) );
		goto err_rw;
	}

	/* Configure as CD-ROM if applicable */
	if ( memcmp ( &scratch->primary.fixed, &primary_check,
		      sizeof ( primary_check ) ) == 0 ) {
		DBGC ( sandev, "SAN %#02x contains an ISO9660 filesystem; "
		       "treating as CD-ROM\n", sandev->drive );
		sandev->blksize_shift = blksize_shift;
		sandev->is_cdrom = 1;
	}

 err_rw:
	free ( scratch );
 err_alloc:
 invalid_blksize:
	return rc;
}

/**
 * Allocate SAN device
 *
 * @v uris		List of URIs
 * @v count		Number of URIs
 * @v priv_size		Size of private data
 * @ret sandev		SAN device, or NULL
 */
struct san_device * alloc_sandev ( struct uri **uris, unsigned int count,
				   size_t priv_size ) {
	struct san_device *sandev;
	struct san_path *sanpath;
	size_t size;
	unsigned int i;

	/* Allocate and initialise structure */
	size = ( sizeof ( *sandev ) + ( count * sizeof ( sandev->path[0] ) ) );
	sandev = zalloc ( size + priv_size );
	if ( ! sandev )
		return NULL;
	ref_init ( &sandev->refcnt, sandev_free );
	intf_init ( &sandev->command, &sandev_command_desc, &sandev->refcnt );
	timer_init ( &sandev->timer, sandev_command_expired, &sandev->refcnt );
	sandev->priv = ( ( ( void * ) sandev ) + size );
	sandev->paths = count;
	INIT_LIST_HEAD ( &sandev->opened );
	INIT_LIST_HEAD ( &sandev->closed );
	for ( i = 0 ; i < count ; i++ ) {
		sanpath = &sandev->path[i];
		sanpath->sandev = sandev;
		sanpath->index = i;
		sanpath->uri = uri_get ( uris[i] );
		list_add_tail ( &sanpath->list, &sandev->closed );
		intf_init ( &sanpath->block, &sanpath_block_desc,
			    &sandev->refcnt );
		process_init_stopped ( &sanpath->process, &sanpath_process_desc,
				       &sandev->refcnt );
		sanpath->path_rc = -EINPROGRESS;
	}

	return sandev;
}

/**
 * Register SAN device
 *
 * @v sandev		SAN device
 * @v drive		Drive number
 * @v flags		Flags
 * @ret rc		Return status code
 */
int register_sandev ( struct san_device *sandev, unsigned int drive,
		      unsigned int flags ) {
	int rc;

	/* Check that drive number is not in use */
	if ( sandev_find ( drive ) != NULL ) {
		DBGC ( sandev, "SAN %#02x is already in use\n", drive );
		rc = -EADDRINUSE;
		goto err_in_use;
	}

	/* Record drive number and flags */
	sandev->drive = drive;
	sandev->flags = flags;

	/* Check that device is capable of being opened (i.e. that all
	 * URIs are well-formed and that at least one path is
	 * working).
	 */
	if ( ( rc = sandev_reopen ( sandev ) ) != 0 )
		goto err_reopen;

	/* Describe device */
	if ( ( rc = sandev_describe ( sandev ) ) != 0 )
		goto err_describe;

	/* Read device capacity */
	if ( ( rc = sandev_command ( sandev, sandev_command_read_capacity,
				     NULL ) ) != 0 )
		goto err_capacity;

	/* Configure as a CD-ROM, if applicable */
	if ( ( rc = sandev_parse_iso9660 ( sandev ) ) != 0 )
		goto err_iso9660;

	/* Add to list of SAN devices */
	list_add_tail ( &sandev->list, &san_devices );
	DBGC ( sandev, "SAN %#02x registered\n", sandev->drive );

	return 0;

	list_del ( &sandev->list );
 err_iso9660:
 err_capacity:
 err_describe:
 err_reopen:
	sandev_restart ( sandev, rc );
	sandev_undescribe ( sandev );
 err_in_use:
	return rc;
}

/**
 * Unregister SAN device
 *
 * @v sandev		SAN device
 */
void unregister_sandev ( struct san_device *sandev ) {

	/* Sanity check */
	assert ( ! timer_running ( &sandev->timer ) );

	/* Remove from list of SAN devices */
	list_del ( &sandev->list );

	/* Shut down interfaces */
	sandev_restart ( sandev, 0 );

	/* Remove ACPI descriptors */
	sandev_undescribe ( sandev );

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

/** The "san-retries" setting */
const struct setting san_retries_setting __setting ( SETTING_SANBOOT_EXTRA,
						     san-retries ) = {
	.name = "san-retries",
	.description = "SAN retry count",
	.tag = DHCP_EB_SAN_RETRY,
	.type = &setting_type_int8,
};

/**
 * Apply SAN boot settings
 *
 * @ret rc		Return status code
 */
static int sandev_apply ( void ) {

	/* Apply "san-retries" setting */
	if ( fetch_uint_setting ( NULL, &san_retries_setting,
				  &san_retries ) < 0 ) {
		san_retries = SAN_DEFAULT_RETRIES;
	}

	return 0;
}

/** Settings applicator */
struct settings_applicator sandev_applicator __settings_applicator = {
	.apply = sandev_apply,
};
