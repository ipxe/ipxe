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

#include <stddef.h>
#include <string.h>
#include <byteswap.h>
#include <gpxe/blockdev.h>
#include <gpxe/scsi.h>

/** @file
 *
 * SCSI block device
 *
 */

static inline __attribute__ (( always_inline )) struct scsi_device *
block_to_scsi ( struct block_device *blockdev ) {
	return container_of ( blockdev, struct scsi_device, blockdev );
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
	return scsi->command ( scsi, command );
}

/**
 * Read block from SCSI device
 *
 * @v blockdev		Block device
 * @v block		LBA block number
 * @v buffer		Data buffer
 * @ret rc		Return status code
 */
static int scsi_read ( struct block_device *blockdev, uint64_t block,
		       void *buffer ) {
	struct scsi_device *scsi = block_to_scsi ( blockdev );
	struct scsi_command command;
	struct scsi_cdb_read_16 *cdb = &command.cdb.read16;

	/* Issue READ (16) */
	memset ( &command, 0, sizeof ( command ) );
	cdb->opcode = SCSI_OPCODE_READ_16;
	cdb->lba = cpu_to_be64 ( block );
	cdb->len = cpu_to_be32 ( 1 ); /* always a single block */
	command.data_in = buffer;
	command.data_in_len = blockdev->blksize;
	return scsi_command ( scsi, &command );
}

/**
 * Write block to SCSI device
 *
 * @v blockdev		Block device
 * @v block		LBA block number
 * @v buffer		Data buffer
 * @ret rc		Return status code
 */
static int scsi_write ( struct block_device *blockdev, uint64_t block,
			const void *buffer ) {
	struct scsi_device *scsi = block_to_scsi ( blockdev );
	struct scsi_command command;
	struct scsi_cdb_write_16 *cdb = &command.cdb.write16;

	/* Issue WRITE (16) */
	memset ( &command, 0, sizeof ( command ) );
	cdb->opcode = SCSI_OPCODE_WRITE_16;
	cdb->lba = cpu_to_be64 ( block );
	cdb->len = cpu_to_be32 ( 1 ); /* always a single block */
	command.data_out = buffer;
	command.data_out_len = blockdev->blksize;
	return scsi_command ( scsi, &command );
}

/**
 * Read capacity of SCSI device
 *
 * @v blockdev		Block device
 * @ret rc		Return status code
 */
static int scsi_read_capacity ( struct block_device *blockdev ) {
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
	command.data_in = &capacity;
	command.data_in_len = sizeof ( capacity );

	if ( ( rc = scsi_command ( scsi, &command ) ) != 0 )
		return rc;

	/* Fill in block device fields */
	blockdev->blksize = be32_to_cpu ( capacity.blksize );
	blockdev->blocks = ( be64_to_cpu ( capacity.lba ) + 1 );
	return 0;
}

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
	/** Fill in read and write methods, and get device capacity */
	scsi->blockdev.read = scsi_read;
	scsi->blockdev.write = scsi_write;
	return scsi_read_capacity ( &scsi->blockdev );
}
