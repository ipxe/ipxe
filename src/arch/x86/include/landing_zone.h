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
#define SLB_SIZE ( 64 * 1024 )

int lz_set_bzimage ( struct image *image, userptr_t zeropage, physaddr_t tgt );

#endif /* _LANDING_ZONE_H */

