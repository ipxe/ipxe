#ifndef _GPXE_MEMMAP_H
#define _GPXE_MEMMAP_H

#include <stdint.h>

/**
 * @file
 *
 * Memory mapping
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

/** A usable memory region */
struct memory_region {
	/** Physical start address */
	uint64_t start;
	/** Physical end address */
	uint64_t end;
};

/** Maximum number of memory regions we expect to encounter */
#define MAX_MEMORY_REGIONS 8

/** A memory map */
struct memory_map {
	/** Memory regions */
	struct memory_region regions[MAX_MEMORY_REGIONS];
	/** Number of used regions */
	unsigned int count;
};

extern void get_memmap ( struct memory_map *memmap );

#endif /* _GPXE_MEMMAP_H */
