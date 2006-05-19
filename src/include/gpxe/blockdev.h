#ifndef _GPXE_BLOCKDEV_H
#define _GPXE_BLOCKDEV_H

/**
 * @file
 *
 * Block devices
 *
 */

#include <gpxe/uaccess.h>

/** A block device */
struct block_device {
	/** Block size */
	size_t blksize;
	/** Total number of blocks */
	uint64_t blocks;
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

#endif /* _GPXE_BLOCKDEV_H */
