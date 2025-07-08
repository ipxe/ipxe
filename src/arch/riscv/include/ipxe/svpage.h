#ifndef _IPXE_SVPAGE_H
#define _IPXE_SVPAGE_H

/** @file
 *
 * Supervisor page table management
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

#ifdef IOMAP_SVPAGE
#define IOMAP_PREFIX_svpage
#else
#define IOMAP_PREFIX_svpage __svpage_
#endif

static inline __always_inline unsigned long
IOMAP_INLINE ( svpage, io_to_bus ) ( volatile const void *io_addr ) {
	/* Not easy to do; just return the CPU address for debugging purposes */
	return ( ( intptr_t ) io_addr );
}

/** Base virtual address for coherent DMA mappings
 *
 * The 64-bit page table includes an uncached mapping of the 32-bit
 * address space at this virtual address.
 */
#define SVPAGE_DMA32 0xffffffc000000000ULL

#endif /* _IPXE_SVPAGE_H */
