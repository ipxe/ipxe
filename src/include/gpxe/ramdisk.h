#ifndef _GPXE_RAMDISK_H
#define _GPXE_RAMDISK_H

/**
 * @file
 *
 * RAM disks
 *
 */

#include <gpxe/uaccess.h>
#include <gpxe/blockdev.h>

struct ramdisk {
	struct block_device blockdev;
	userptr_t data;
};

#endif /* _GPXE_RAMDISK_H */
