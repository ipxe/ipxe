#ifndef _IPXE_FDTMEM_H
#define _IPXE_FDTMEM_H

/** @file
 *
 * Flattened Device Tree memory map
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

#ifdef MEMMAP_FDT
#define MEMMAP_PREFIX_fdt
#else
#define MEMMAP_PREFIX_fdt __fdt_
#endif

/**
 * Synchronise in-use regions with the externally visible system memory map
 *
 */
static inline __attribute__ (( always_inline )) void
MEMMAP_INLINE ( fdt, memmap_sync ) ( void ) {
	/* Nothing to do */
}

struct fdt_header;

extern physaddr_t fdtmem_relocate ( struct fdt_header *hdr, physaddr_t max );
extern int fdtmem_register ( struct fdt_header *hdr, physaddr_t max );

#endif /* _IPXE_FDTMEM_H */
