#ifndef _BITS_PROFILE_H
#define _BITS_PROFILE_H

/** @file
 *
 * Profiling
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

extern uint32_t pmccntr_status;

/**
 * Get profiling timestamp
 *
 * @ret timestamp	Timestamp
 */
static inline __attribute__ (( always_inline )) unsigned long
profile_timestamp ( void ) {
	uint32_t cycles;

	/* Read cycle counter */
	__asm__ __volatile__ ( /* Check PMCCNTR status */
			       "tst %0, %0\n\t"
			       /* Check availability if not yet known */
			       "it mi\n\t"
			       "blxmi pmccntr_check\n\t"
			       /* Read PMCCNTR if available */
			       "it ne\n\t"
			       "mrcne p15, 0, %0, c9, c13, 0\n\t"
			       "\n1:\n\t"
			       : "=r" ( cycles )
			       : "0" ( pmccntr_status )
			       : "cc", "lr" );
	return cycles;
}

#endif /* _BITS_PROFILE_H */
