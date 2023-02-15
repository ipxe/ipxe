#ifndef _IPXE_RDRAND_H
#define _IPXE_RDRAND_H

/** @file
 *
 * Hardware random number generator
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/drbg.h>

#ifdef ENTROPY_RDRAND
#define ENTROPY_PREFIX_rdrand
#else
#define ENTROPY_PREFIX_rdrand __rdrand_
#endif

/**
 * min-entropy per sample
 *
 * @ret min_entropy	min-entropy of each sample
 */
static inline __always_inline min_entropy_t
ENTROPY_INLINE ( rdrand, min_entropy_per_sample ) ( void ) {

	/* Data returned by RDRAND is theoretically full entropy, up
	 * to a security strength of 128 bits.
	 */
	if ( DRBG_SECURITY_STRENGTH > 128 )
		return 0;
	return MIN_ENTROPY ( 8 * sizeof ( noise_sample_t ) );
}

#endif /* _IPXE_RDRAND_H */
