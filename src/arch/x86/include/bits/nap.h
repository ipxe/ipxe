#ifndef _BITS_NAP_H
#define _BITS_NAP_H

/** @file
 *
 * x86-specific CPU sleeping API implementations
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/bios_nap.h>

/**
 * Sleep until next CPU interrupt
 *
 */
static inline __attribute__ (( always_inline )) void cpu_halt ( void ) {
	__asm__ __volatile__ ( "hlt" );
}

#endif /* _BITS_NAP_H */
