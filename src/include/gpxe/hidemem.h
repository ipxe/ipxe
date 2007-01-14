#ifndef _GPXE_HIDEMEM_H
#define _GPXE_HIDEMEM_H

/**
 * @file
 *
 * Hidden memory regions
 *
 */

/**
 * Unique IDs for hidden regions
 */
enum hidemem_region_id {
	TEXT = 0,
	BASEMEM,
	EXTMEM,
};

extern void hide_etherboot();
extern void unhide_etherboot();
extern void hide_region ( unsigned int region_id, physaddr_t start,
			  physaddr_t end );

#endif /* _GPXE_HIDEMEM_H */
