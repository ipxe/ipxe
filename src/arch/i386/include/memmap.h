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

extern void get_memmap ( struct memory_region *memmap, unsigned int entries );

#endif /* _MEMMAP_H */
