#ifndef _IPXE_NULL_MEMMAP_H
#define _IPXE_NULL_MEMMAP_H

#include <stdint.h>

/** @file
 *
 * Null system memory map API
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef MEMMAP_NULL
#define MEMMAP_PREFIX_null
#else
#define MEMMAP_PREFIX_null __null_
#endif

/**
 * Describe memory region from system memory map
 *
 * @v min		Minimum address
 * @v hide		Hide in-use regions from the memory map
 * @v region		Region descriptor to fill in
 */
static inline __attribute__ (( always_inline )) void
MEMMAP_INLINE ( null, memmap_describe ) ( uint64_t min, int hide __unused,
					  struct memmap_region *region ) {

	/* Initialise region as empty */
	memmap_init ( min, region );
}

/**
 * Synchronise in-use regions with the externally visible system memory map
 *
 */
static inline __attribute__ (( always_inline )) void
MEMMAP_INLINE ( null, memmap_sync ) ( void ) {
	/* Nothing to do */
}

#endif /* _IPXE_NULL_MEMMAP_H */
