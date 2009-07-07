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
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <byteswap.h>
#include <gpxe/blockdev.h>
#include <gpxe/process.h>
#include <gpxe/ata.h>

/** @file
 *
 * ATA block device
 *
 */

static inline __attribute__ (( always_inline )) struct ata_device *
block_to_ata ( struct block_device *blockdev ) {
	return container_of ( blockdev, struct ata_device, blockdev );
}

/**
 * Issue ATA command
 *
 * @v ata		ATA device
 * @v command		ATA command
 * @ret rc		Return status code
 */
static inline __attribute__ (( always_inline )) int
ata_command ( struct ata_device *ata, struct ata_command *command ) {
	int rc;

	DBG ( "ATA cmd %02x dev %02x LBA%s %llx count %04x\n",
	      command->cb.cmd_stat, command->cb.device,
	      ( command->cb.lba48 ? "48" : "" ),
	      ( unsigned long long ) command->cb.lba.native,
	      command->cb.count.native );

	/* Flag command as in-progress */
	command->rc = -EINPROGRESS;

	/* Issue ATA command */
	if ( ( rc = ata->command ( ata, command ) ) != 0 ) {
		/* Something went wrong with the issuing mechanism */
		DBG ( "ATA could not issue command: %s\n", strerror ( rc ) );
		return rc;
	}

	/* Wait for command to complete */
	while ( command->rc == -EINPROGRESS )
		step();
	if ( ( rc = command->rc ) != 0 ) {
		/* Something went wrong with the command execution */
		DBG ( "ATA command failed: %s\n", strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Read block from ATA device
 *
 * @v blockdev		Block device
 * @v block		LBA block number
 * @v count		Block count
 * @v buffer		Data buffer
 * @ret rc		Return status code
 */
static int ata_read ( struct block_device *blockdev, uint64_t block,
		      unsigned long count, userptr_t buffer ) {
	struct ata_device *ata = block_to_ata ( blockdev );
	struct ata_command command;

	memset ( &command, 0, sizeof ( command ) );
	command.cb.lba.native = block;
	command.cb.count.native = count;
	command.cb.device = ( ata->device | ATA_DEV_OBSOLETE | ATA_DEV_LBA );
	command.cb.lba48 = ata->lba48;
	if ( ! ata->lba48 )
		command.cb.device |= command.cb.lba.bytes.low_prev;
	command.cb.cmd_stat = ( ata->lba48 ? ATA_CMD_READ_EXT : ATA_CMD_READ );
	command.data_in = buffer;
	return ata_command ( ata, &command );
}

/**
 * Write block to ATA device
 *
 * @v blockdev		Block device
 * @v block		LBA block number
 * @v count		Block count
 * @v buffer		Data buffer
 * @ret rc		Return status code
 */
static int ata_write ( struct block_device *blockdev, uint64_t block,
		       unsigned long count, userptr_t buffer ) {
	struct ata_device *ata = block_to_ata ( blockdev );
	struct ata_command command;
	
	memset ( &command, 0, sizeof ( command ) );
	command.cb.lba.native = block;
	command.cb.count.native = count;
	command.cb.device = ( ata->device | ATA_DEV_OBSOLETE | ATA_DEV_LBA );
	command.cb.lba48 = ata->lba48;
	if ( ! ata->lba48 )
		command.cb.device |= command.cb.lba.bytes.low_prev;
	command.cb.cmd_stat = ( ata->lba48 ?
				ATA_CMD_WRITE_EXT : ATA_CMD_WRITE );
	command.data_out = buffer;
	return ata_command ( ata, &command );
}

/**
 * Identify ATA device
 *
 * @v blockdev		Block device
 * @ret rc		Return status code
 */
static int ata_identify ( struct block_device *blockdev ) {
	struct ata_device *ata = block_to_ata ( blockdev );
	struct ata_command command;
	struct ata_identity identity;
	int rc;

	/* Issue IDENTIFY */
	memset ( &command, 0, sizeof ( command ) );
	command.cb.count.native = 1;
	command.cb.device = ( ata->device | ATA_DEV_OBSOLETE | ATA_DEV_LBA );
	command.cb.cmd_stat = ATA_CMD_IDENTIFY;
	command.data_in = virt_to_user ( &identity );
	linker_assert ( sizeof ( identity ) == ATA_SECTOR_SIZE,
			__ata_identity_bad_size__ );
	if ( ( rc = ata_command ( ata, &command ) ) != 0 )
		return rc;

	/* Fill in block device parameters */
	blockdev->blksize = ATA_SECTOR_SIZE;
	if ( identity.supports_lba48 & cpu_to_le16 ( ATA_SUPPORTS_LBA48 ) ) {
		ata->lba48 = 1;
		blockdev->blocks = le64_to_cpu ( identity.lba48_sectors );
	} else {
		blockdev->blocks = le32_to_cpu ( identity.lba_sectors );
	}
	return 0;
}

static struct block_device_operations ata_operations = {
	.read	= ata_read,
	.write	= ata_write
};

/**
 * Initialise ATA device
 *
 * @v ata		ATA device
 * @ret rc		Return status code
 *
 * Initialises an ATA device.  The ata_device::command field and the
 * @c ATA_FL_SLAVE portion of the ata_device::flags field must already
 * be filled in.  This function will configure ata_device::blockdev,
 * including issuing an IDENTIFY DEVICE call to determine the block
 * size and total device size.
 */
int init_atadev ( struct ata_device *ata ) {
	/** Fill in read and write methods, and get device capacity */
	ata->blockdev.op = &ata_operations;
	return ata_identify ( &ata->blockdev );
}
