#ifndef _INITRD_H
#define _INITRD_H

/**
 * @file
 *
 * Linux initrd image format
 *
 */

#include <gpxe/image.h>
extern struct image_type initrdimage_image_type __image_type ( PROBE_NORMAL );

#endif /* _INITRD_H */
