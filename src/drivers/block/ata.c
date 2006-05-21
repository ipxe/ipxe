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
	DBG ( "ATA cmd %02x dev %02x fl %02x LBA %llx count %04x\n",
	      command.cb.cmd_stat, command.cb.device, command.cb.flags,
	      ( unsigned long long ) command.cb.lba.native,
	      command.cb.count.native );

	return ata->command ( ata, command );	
}

/**
 * Read block from / write block to ATA device
 *
 * @v write		Write flag (ATA_FL_WRITE or 0)
 * @v blockdev		Block device
 * @v block		LBA block number
 * @v count		Block count
 * @v buffer		Data buffer
 * @ret rc		Return status code
 */
static __attribute__ (( regparm ( 1 ) )) int
ata_rw ( int write, struct block_device *blockdev, uint64_t block,
	 unsigned long count, userptr_t buffer ) {
	struct ata_device *ata = block_to_ata ( blockdev );
	struct ata_command command;
	int lba48 = ( ata->flags & ATA_FL_LBA48 );

	memset ( &command, 0, sizeof ( command ) );
	command.cb.lba.native = block;
	command.cb.count.native = count;
	command.cb.device = ( ata->flags | ATA_DEV_OBSOLETE | ATA_DEV_LBA );
	command.cb.flags = ( ata->flags | write );
	command.cb.cmd_stat = ( write ? ATA_CMD_WRITE : ATA_CMD_READ );
	if ( lba48 ) {
		command.cb.cmd_stat |= ATA_CMD_EXT;
	} else {
		command.cb.device |= command.cb.lba.bytes.low_prev;
	}
	command.data = buffer;
	command.data_len = ( count * blockdev->blksize );
	return ata_command ( ata, &command );
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
	/* Pass through to ata_rw().  Since ata_rw is regparm(1), this
	 * is extremely efficient; just a mov and a jmp.
	 */
	return ata_rw ( 0, blockdev, block, count, buffer );
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
	/* Pass through to ata_rw().  Since ata_rw is regparm(1), this
	 * is extremely efficient; just a mov and a jmp.
	 */
	return ata_rw ( ATA_FL_WRITE, blockdev, block, count, buffer );
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
	command.cb.device = ( ata->flags | ATA_DEV_OBSOLETE | ATA_DEV_LBA );
	command.cb.cmd_stat = ATA_CMD_IDENTIFY;
	command.data = virt_to_user ( &identity );
	command.data_len = sizeof ( identity );
	if ( ( rc = ata_command ( ata, &command ) ) != 0 )
		return rc;

	/* Fill in block device parameters */
	blockdev->blksize = ATA_SECTOR_SIZE;
	if ( identity.supports_lba48 & cpu_to_le16 ( ATA_SUPPORTS_LBA48 ) ) {
		ata->flags |= ATA_FL_LBA48;
		blockdev->blocks = le64_to_cpu ( identity.lba48_sectors );
	} else {
		blockdev->blocks = le32_to_cpu ( identity.lba_sectors );
	}
	return 0;
}

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
	ata->blockdev.read = ata_read;
	ata->blockdev.write = ata_write;
	return ata_identify ( &ata->blockdev );
}
