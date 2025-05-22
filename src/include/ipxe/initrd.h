#ifndef _IPXE_INITRD_H
#define _IPXE_INITRD_H

/** @file
 *
 * Initial ramdisk (initrd) reshuffling
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

extern void initrd_reshuffle ( physaddr_t bottom );
extern int initrd_reshuffle_check ( size_t len, physaddr_t bottom );

/** Initial ramdisk chunk alignment */
#define INITRD_ALIGN 4096

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

#endif /* _IPXE_INITRD_H */
