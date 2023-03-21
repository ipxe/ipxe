#ifndef _USR_IMGARCHIVE_H
#define _USR_IMGARCHIVE_H

/** @file
 *
 * Archive image management
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/image.h>

extern int imgextract ( struct image *image, const char *name );

#endif /* _USR_IMGARCHIVE_H */
