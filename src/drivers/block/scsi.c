/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <byteswap.h>
#include <errno.h>
#include <gpxe/blockdev.h>
#include <gpxe/process.h>
#include <gpxe/scsi.h>

/** @file
 *
 * SCSI block device
 *
 */

/** Maximum number of dummy "read capacity (10)" operations
 *
 * These are issued at connection setup to draw out various useless
 * power-on messages.
 */
#define SCSI_MAX_DUMMY_READ_CAP 10

static inline __attribute__ (( always_inline )) struct scsi_device *
block_to_scsi ( struct block_device *blockdev ) {
	return container_of ( blockdev, struct scsi_device, blockdev );
}

/**
 * Handle SCSI command with no backing device
 *
 * @v scsi		SCSI device
 * @v command		SCSI command
 * @ret rc		Return status code
 */
int scsi_detached_command ( struct scsi_device *scsi __unused,
			    struct scsi_command *command __unused ) {
	return -ENODEV;
}

/**
 * Issue SCSI command
 *
 * @v scsi		SCSI device
 * @v command		SCSI command
 * @ret rc		Return status code
 */
static int scsi_command ( struct scsi_device *scsi,
			  struct scsi_command *command ) {
	int rc;

	DBGC2 ( scsi, "SCSI %p " SCSI_CDB_FORMAT "\n",
		scsi, SCSI_CDB_DATA ( command->cdb ) );

	/* Clear sense response code before issuing command */
	command->sense_response = 0;

	/* Flag command as in-progress */
	command->rc = -EINPROGRESS;

	/* Issue SCSI command */
	if ( ( rc = scsi->command ( scsi, command ) ) != 0 ) {
		/* Something went wrong with the issuing mechanism */
		DBGC ( scsi, "SCSI %p " SCSI_CDB_FORMAT " err %s\n",
		       scsi, SCSI_CDB_DATA ( command->cdb ), strerror ( rc ) );
		return rc;
	}

	/* Wait for command to complete */
	while ( command->rc == -EINPROGRESS )
		step();
	if ( ( rc = command->rc ) != 0 ) {
		/* Something went wrong with the command execution */
		DBGC ( scsi, "SCSI %p " SCSI_CDB_FORMAT " err %s\n",
		       scsi, SCSI_CDB_DATA ( command->cdb ), strerror ( rc ) );
		return rc;
	}

	/* Check for SCSI errors */
	if ( command->status != 0 ) {
		DBGC ( scsi, "SCSI %p " SCSI_CDB_FORMAT " status %02x sense "
		       "%02x\n", scsi, SCSI_CDB_DATA ( command->cdb ),
		       command->status, command->sense_response );
		return -EIO;
	}

	return 0;
}

/**
 * Read block from SCSI device using READ (10)
 *
 * @v blockdev		Block device
 * @v block		LBA block number
 * @v count		Block count
 * @v buffer		Data buffer
 * @ret rc		Return status code
 */
static int scsi_read_10 ( struct block_device *blockdev, uint64_t block,
			  unsigned long count, userptr_t buffer ) {
	struct scsi_device *scsi = block_to_scsi ( blockdev );
	struct scsi_command command;
	struct scsi_cdb_read_10 *cdb = &command.cdb.read10;

	/* Issue READ (10) */
	memset ( &command, 0, sizeof ( command ) );
	cdb->opcode = SCSI_OPCODE_READ_10;
	cdb->lba = cpu_to_be32 ( block );
	cdb->len = cpu_to_be16 ( count );
	command.data_in = buffer;
	command.data_in_len = ( count * blockdev->blksize );
	return scsi_command ( scsi, &command );
}

/**
 * Read block from SCSI device using READ (16)
 *
 * @v blockdev		Block device
 * @v block		LBA block number
 * @v count		Block count
 * @v buffer		Data buffer
 * @ret rc		Return status code
 */
static int scsi_read_16 ( struct block_device *blockdev, uint64_t block,
			  unsigned long count, userptr_t buffer ) {
	struct scsi_device *scsi = block_to_scsi ( blockdev );
	struct scsi_command command;
	struct scsi_cdb_read_16 *cdb = &command.cdb.read16;

	/* Issue READ (16) */
	memset ( &command, 0, sizeof ( command ) );
	cdb->opcode = SCSI_OPCODE_READ_16;
	cdb->lba = cpu_to_be64 ( block );
	cdb->len = cpu_to_be32 ( count );
	command.data_in = buffer;
	command.data_in_len = ( count * blockdev->blksize );
	return scsi_command ( scsi, &command );
}

/**
 * Write block to SCSI device using WRITE (10)
 *
 * @v blockdev		Block device
 * @v block		LBA block number
 * @v count		Block count
 * @v buffer		Data buffer
 * @ret rc		Return status code
 */
static int scsi_write_10 ( struct block_device *blockdev, uint64_t block,
			   unsigned long count, userptr_t buffer ) {
	struct scsi_device *scsi = block_to_scsi ( blockdev );
	struct scsi_command command;
	struct scsi_cdb_write_10 *cdb = &command.cdb.write10;

	/* Issue WRITE (10) */
	memset ( &command, 0, sizeof ( command ) );
	cdb->opcode = SCSI_OPCODE_WRITE_10;
	cdb->lba = cpu_to_be32 ( block );
	cdb->len = cpu_to_be16 ( count );
	command.data_out = buffer;
	command.data_out_len = ( count * blockdev->blksize );
	return scsi_command ( scsi, &command );
}

/**
 * Write block to SCSI device using WRITE (16)
 *
 * @v blockdev		Block device
 * @v block		LBA block number
 * @v count		Block count
 * @v buffer		Data buffer
 * @ret rc		Return status code
 */
static int scsi_write_16 ( struct block_device *blockdev, uint64_t block,
			   unsigned long count, userptr_t buffer ) {
	struct scsi_device *scsi = block_to_scsi ( blockdev );
	struct scsi_command command;
	struct scsi_cdb_write_16 *cdb = &command.cdb.write16;

	/* Issue WRITE (16) */
	memset ( &command, 0, sizeof ( command ) );
	cdb->opcode = SCSI_OPCODE_WRITE_16;
	cdb->lba = cpu_to_be64 ( block );
	cdb->len = cpu_to_be32 ( count );
	command.data_out = buffer;
	command.data_out_len = ( count * blockdev->blksize );
	return scsi_command ( scsi, &command );
}

/**
 * Read capacity of SCSI device via READ CAPACITY (10)
 *
 * @v blockdev		Block device
 * @ret rc		Return status code
 */
static int scsi_read_capacity_10 ( struct block_device *blockdev ) {
	struct scsi_device *scsi = block_to_scsi ( blockdev );
	struct scsi_command command;
	struct scsi_cdb_read_capacity_10 *cdb = &command.cdb.readcap10;
	struct scsi_capacity_10 capacity;
	int rc;

	/* Issue READ CAPACITY (10) */
	memset ( &command, 0, sizeof ( command ) );
	cdb->opcode = SCSI_OPCODE_READ_CAPACITY_10;
	command.data_in = virt_to_user ( &capacity );
	command.data_in_len = sizeof ( capacity );

	if ( ( rc = scsi_command ( scsi, &command ) ) != 0 )
		return rc;

	/* Fill in block device fields */
	blockdev->blksize = be32_to_cpu ( capacity.blksize );
	blockdev->blocks = ( be32_to_cpu ( capacity.lba ) + 1 );

	return 0;
}

/**
 * Read capacity of SCSI device via READ CAPACITY (16)
 *
 * @v blockdev		Block device
 * @ret rc		Return status code
 */
static int scsi_read_capacity_16 ( struct block_device *blockdev ) {
	struct scsi_device *scsi = block_to_scsi ( blockdev );
	struct scsi_command command;
	struct scsi_cdb_read_capacity_16 *cdb = &command.cdb.readcap16;
	struct scsi_capacity_16 capacity;
	int rc;

	/* Issue READ CAPACITY (16) */
	memset ( &command, 0, sizeof ( command ) );
	cdb->opcode = SCSI_OPCODE_SERVICE_ACTION_IN;
	cdb->service_action = SCSI_SERVICE_ACTION_READ_CAPACITY_16;
	cdb->len = cpu_to_be32 ( sizeof ( capacity ) );
	command.data_in = virt_to_user ( &capacity );
	command.data_in_len = sizeof ( capacity );

	if ( ( rc = scsi_command ( scsi, &command ) ) != 0 )
		return rc;

	/* Fill in block device fields */
	blockdev->blksize = be32_to_cpu ( capacity.blksize );
	blockdev->blocks = ( be64_to_cpu ( capacity.lba ) + 1 );
	return 0;
}

static struct block_device_operations scsi_operations_16 = {
	.read	= scsi_read_16,
	.write	= scsi_write_16,
};

static struct block_device_operations scsi_operations_10 = {
	.read	= scsi_read_10,
	.write	= scsi_write_10,
};

/**
 * Initialise SCSI device
 *
 * @v scsi		SCSI device
 * @ret rc		Return status code
 *
 * Initialises a SCSI device.  The scsi_device::command and
 * scsi_device::lun fields must already be filled in.  This function
 * will configure scsi_device::blockdev, including issuing a READ
 * CAPACITY call to determine the block size and total device size.
 */
int init_scsidev ( struct scsi_device *scsi ) {
	unsigned int i;
	int rc;

	/* Issue some theoretically extraneous READ CAPACITY (10)
	 * commands, solely in order to draw out the "CHECK CONDITION
	 * (power-on occurred)", "CHECK CONDITION (reported LUNs data
	 * has changed)" etc. that some dumb targets insist on sending
	 * as an error at start of day.  The precise command that we
	 * use is unimportant; we just need to provide the target with
	 * an opportunity to send its responses.
	 */
	for ( i = 0 ; i < SCSI_MAX_DUMMY_READ_CAP ; i++ ) {
		if ( ( rc = scsi_read_capacity_10 ( &scsi->blockdev ) ) == 0 )
			break;
		DBGC ( scsi, "SCSI %p ignoring start-of-day error (#%d)\n",
		       scsi, ( i + 1 ) );
	}

	/* Try READ CAPACITY (10), which is a mandatory command, first. */
	scsi->blockdev.op = &scsi_operations_10;
	if ( ( rc = scsi_read_capacity_10 ( &scsi->blockdev ) ) != 0 ) {
		DBGC ( scsi, "SCSI %p could not READ CAPACITY (10): %s\n",
		       scsi, strerror ( rc ) );
		return rc;
	}

	/* If capacity range was exceeded (i.e. capacity.lba was
	 * 0xffffffff, meaning that blockdev->blocks is now zero), use
	 * READ CAPACITY (16) instead.  READ CAPACITY (16) is not
	 * mandatory, so we can't just use it straight off.
	 */
	if ( scsi->blockdev.blocks == 0 ) {
		scsi->blockdev.op = &scsi_operations_16;
		if ( ( rc = scsi_read_capacity_16 ( &scsi->blockdev ) ) != 0 ){
			DBGC ( scsi, "SCSI %p could not READ CAPACITY (16): "
			       "%s\n", scsi, strerror ( rc ) );
			return rc;
		}
	}

	DBGC ( scsi, "SCSI %p using READ/WRITE (%d) commands\n", scsi,
	       ( ( scsi->blockdev.op == &scsi_operations_10 ) ? 10 : 16 ) );
	DBGC ( scsi, "SCSI %p capacity is %ld MB (%#llx blocks)\n", scsi,
	       ( ( unsigned long ) ( scsi->blockdev.blocks >> 11 ) ),
	       scsi->blockdev.blocks );

	return 0;
}

/**
 * Parse SCSI LUN
 *
 * @v lun_string	LUN string representation
 * @v lun		LUN to fill in
 * @ret rc		Return status code
 */
int scsi_parse_lun ( const char *lun_string, struct scsi_lun *lun ) {
	char *p;
	int i;

	memset ( lun, 0, sizeof ( *lun ) );
	if ( lun_string ) {
		p = ( char * ) lun_string;
		for ( i = 0 ; i < 4 ; i++ ) {
			lun->u16[i] = htons ( strtoul ( p, &p, 16 ) );
			if ( *p == '\0' )
				break;
			if ( *p != '-' )
				return -EINVAL;
			p++;
		}
		if ( *p )
			return -EINVAL;
	}

	return 0;
}
