#ifndef _LANDING_ZONE_H
#define _LANDING_ZONE_H

/** @file
 *
 * Landing Zone defines
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/image.h>

/** Minimum alignment for Landing Zone
 */
#define LZ_ALIGN ( 64 * 1024 )
#define LZ_SIZE ( 64 * 1024 )

void landing_zone_set_bzimage ( struct image *lz, userptr_t zeropage );

#endif /* _LANDING_ZONE_H */

