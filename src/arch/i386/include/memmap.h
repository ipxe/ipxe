#ifndef _MEMMAP_H
#define _MEMMAP_H

#include <stdint.h>

/**
 * @file
 *
 * Memory mapping
 *
 */

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
	struct memory_region regions[MAX_MEMORY_REGIONS];
};

extern void get_memmap ( struct memory_map *memmap );

#endif /* _MEMMAP_H */
