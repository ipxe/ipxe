#ifndef _IPXE_INT15_H
#define _IPXE_INT15_H

/** @file
 *
 * INT15-based memory map
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef MEMMAP_INT15
#define MEMMAP_PREFIX_int15
#else
#define MEMMAP_PREFIX_int15 __int15_
#endif

extern void int15_intercept ( int intercept );
extern void hide_basemem ( void );

#endif /* _IPXE_INT15_H */
