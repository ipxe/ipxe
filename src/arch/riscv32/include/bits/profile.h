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
static inline __attribute__ (( always_inline )) uint64_t
profile_timestamp ( void ) {
	uint32_t cycles_lo;
	uint32_t cycles_hi;
	uint32_t tmp;

	/* Read timestamp counter */
	__asm__ __volatile__ ( "\n1:\n\t"
			       "rdcycleh %1\n\t"
			       "rdcycle %0\n\t"
			       "rdcycleh %2\n\t"
			       "bne %1, %2, 1b\n\t"
			       : "=r" ( cycles_lo ), "=r" ( cycles_hi ),
				 "=r" ( tmp ) );
	return ( ( ( ( uint64_t ) cycles_hi ) << 32 ) | cycles_lo );
}

#endif /* _BITS_PROFILE_H */
