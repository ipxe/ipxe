#ifndef _BITS_NAP_H
#define _BITS_NAP_H

/** @file
 *
 * LoongArch64-specific CPU sleeping API implementations
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/**
 * Sleep until next CPU interrupt
 *
 */
static inline __attribute__ (( always_inline )) void cpu_halt ( void ) {
	__asm__ __volatile__ ( "idle 0" );
}

#endif /* _BITS_NAP_H */
