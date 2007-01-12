#ifndef _GPXE_HIDEMEM_H
#define _GPXE_HIDEMEM_H

/**
 * @file
 *
 * Hidden memory regions
 *
 */

/**
 * A hidden region of Etherboot
 *
 * This represents a region that will be edited out of the system's
 * memory map.
 *
 * This structure is accessed by assembly code, so must not be
 * changed.
 */
struct hidden_region {
	/* Physical start address */
	physaddr_t start;
	/* Physical end address */
	physaddr_t end;
};

/**
 * Unique IDs for hidden regions
 */
enum {
	TEXT = 0,
	BASEMEM,
	EXTMEM,
};

extern struct hidden_region __data16_array ( hidden_regions, [] );
#define hidden_regions __use_data16 ( hidden_regions )

static inline void hide_region ( unsigned int region,
				 physaddr_t start, physaddr_t end ) {
	hidden_regions[region].start = start;
	hidden_regions[region].end = end;
}

#endif /* _GPXE_HIDEMEM_H */
