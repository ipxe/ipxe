#ifndef _IPXE_INITRD_H
#define _IPXE_INITRD_H

/** @file
 *
 * Initial ramdisk (initrd) reshuffling
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/memmap.h>

/** Initial ramdisk chunk alignment */
#define INITRD_ALIGN 4096

extern void initrd_reshuffle ( void );
extern int initrd_region ( size_t len, struct memmap_region *region );
extern size_t initrd_load_all ( void *address );

/**
 * Align initrd length
 *
 * @v len		Length
 * @ret len		Aligned length
 */
static inline __attribute__ (( always_inline )) size_t
initrd_align ( size_t len ) {

	return ( ( len + INITRD_ALIGN - 1 ) & ~( INITRD_ALIGN - 1 ) );
}

/**
 * Get required length for initrds
 *
 * @ret len		Required length
 */
static inline __attribute__ (( always_inline )) size_t
initrd_len ( void ) {

	return initrd_load_all ( NULL );
}

#endif /* _IPXE_INITRD_H */
