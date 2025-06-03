#ifndef _USR_FDTMGMT_H
#define _USR_FDTMGMT_H

/** @file
 *
 * Flattened Device Tree management
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/image.h>

extern int imgfdt ( struct image *image );

#endif /* _USR_FDTMGMT_H */
