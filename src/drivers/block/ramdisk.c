/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <gpxe/blockdev.h>
#include <gpxe/ramdisk.h>

/**
 * @file
 *
 * RAM disks
 *
 */

static inline __attribute__ (( always_inline )) struct ramdisk *
block_to_ramdisk ( struct block_device *blockdev ) {
	return container_of ( blockdev, struct ramdisk, blockdev );
}

/**
 * Read block
 *
 * @v blockdev		Block device
 * @v block		Block number
 * @v count		Block count
 * @v buffer		Data buffer
 * @ret rc		Return status code
 */
static int ramdisk_read ( struct block_device *blockdev, uint64_t block,
			  unsigned long count, userptr_t buffer ) {
	struct ramdisk *ramdisk = block_to_ramdisk ( blockdev );
	unsigned long offset = ( block * blockdev->blksize );
	unsigned long length = ( count * blockdev->blksize );

	DBGC ( ramdisk, "RAMDISK %p reading [%lx,%lx)\n",
	       ramdisk, offset, length );

	memcpy_user ( buffer, 0, ramdisk->data, offset, length );
	return 0;
}

/**
 * Write block
 *
 * @v blockdev		Block device
 * @v block		Block number
 * @v count		Block count
 * @v buffer		Data buffer
 * @ret rc		Return status code
 */
static int ramdisk_write ( struct block_device *blockdev, uint64_t block,
			   unsigned long count, userptr_t buffer ) {
	struct ramdisk *ramdisk = block_to_ramdisk ( blockdev );
	unsigned long offset = ( block * blockdev->blksize );
	unsigned long length = ( count * blockdev->blksize );

	DBGC ( ramdisk, "RAMDISK %p writing [%lx,%lx)\n",
	       ramdisk, offset, length );

	memcpy_user ( ramdisk->data, offset, buffer, 0, length );
	return 0;
}

static struct block_device_operations ramdisk_operations = {
	.read	= ramdisk_read,
	.write	= ramdisk_write
};

int init_ramdisk ( struct ramdisk *ramdisk, userptr_t data, size_t len,
		   unsigned int blksize ) {
	
	if ( ! blksize )
		blksize = 512;

	ramdisk->data = data;
	ramdisk->blockdev.op = &ramdisk_operations;
	ramdisk->blockdev.blksize = blksize;
	ramdisk->blockdev.blocks = ( len / blksize );

	return 0;
}
