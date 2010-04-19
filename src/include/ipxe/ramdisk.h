#ifndef _IPXE_RAMDISK_H
#define _IPXE_RAMDISK_H

/**
 * @file
 *
 * RAM disks
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <ipxe/uaccess.h>
#include <ipxe/blockdev.h>

struct ramdisk {
	struct block_device blockdev;
	userptr_t data;
};

extern int init_ramdisk ( struct ramdisk *ramdisk, userptr_t data, size_t len,
			  unsigned int blksize );

#endif /* _IPXE_RAMDISK_H */
