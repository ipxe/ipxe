#ifndef _IPXE_MEMBLOCK_H
#define _IPXE_MEMBLOCK_H

/** @file
 *
 * Largest memory block
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

extern size_t largest_memblock ( void **start );

#endif /* _IPXE_MEMBLOCK_H */
