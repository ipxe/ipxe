#ifndef _GPXE_INITRD_H
#define _GPXE_INITRD_H

/**
 * @file
 *
 * Linux initrd image format
 *
 */

#include <gpxe/image.h>
extern struct image_type initrd_image_type __image_type ( PROBE_NORMAL );

#endif /* _GPXE_INITRD_H */
