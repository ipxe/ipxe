#ifndef _GPXE_BLOCKDEV_H
#define _GPXE_BLOCKDEV_H

/**
 * @file
 *
 * Block devices
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <gpxe/uaccess.h>

struct block_device;

/** Block device operations */
struct block_device_operations {
	/**
	 * Read block
	 *
	 * @v blockdev	Block device
	 * @v block	Block number
	 * @v count	Block count
	 * @v buffer	Data buffer
	 * @ret rc	Return status code
	 */
	int ( * read ) ( struct block_device *blockdev, uint64_t block,
			 unsigned long count, userptr_t buffer );
	/**
	 * Write block
	 *
	 * @v blockdev	Block device
	 * @v block	Block number
	 * @v count	Block count
	 * @v buffer	Data buffer
	 * @ret rc	Return status code
	 */
	int ( * write ) ( struct block_device *blockdev, uint64_t block,
			  unsigned long count, userptr_t buffer );
};

/** A block device */
struct block_device {
	/** Block device operations */
	struct block_device_operations *op;
	/** Block size */
	size_t blksize;
	/** Total number of blocks */
	uint64_t blocks;
};

#endif /* _GPXE_BLOCKDEV_H */
