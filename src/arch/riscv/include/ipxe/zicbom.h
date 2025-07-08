#ifndef _IPXE_ZICBOM_H
#define _IPXE_ZICBOM_H

/** @file
 *
 * Cache-block management operations (Zicbom)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

extern void cache_clean ( const void *start, size_t len );
extern void cache_invalidate ( void *start, size_t len );

#endif /* _IPXE_ZICBOM_H */
