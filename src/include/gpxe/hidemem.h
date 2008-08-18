#ifndef _GPXE_HIDEMEM_H
#define _GPXE_HIDEMEM_H

/**
 * @file
 *
 * Hidden memory regions
 *
 */

#include <stdint.h>

extern void hide_umalloc ( physaddr_t start, physaddr_t end );

#endif /* _GPXE_HIDEMEM_H */
