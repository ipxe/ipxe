#ifndef _BITS_PROFILE_H
#define _BITS_PROFILE_H

/** @file
 *
 * Profiling
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

/**
 * Get profiling timestamp
 *
 * @ret timestamp	Timestamp
 */
static inline __attribute__ (( always_inline )) unsigned long
profile_timestamp ( void ) {
	unsigned long cycles;

	/* Read timestamp counter */
	__asm__ __volatile__ ( "rdcycle %0" : "=r" ( cycles ) );
	return cycles;
}

#endif /* _BITS_PROFILE_H */
