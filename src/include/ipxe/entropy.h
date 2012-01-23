#ifndef _IPXE_ENTROPY_H
#define _IPXE_ENTROPY_H

/** @file
 *
 * Entropy source
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <assert.h>

/** min-entropy per entropy sample
 *
 * min-entropy is defined in ANS X9.82 Part 1-2006 Section 8.3 and in
 * NIST SP 800-90 Appendix C.3 as
 *
 *    H_min = -log2 ( p_max )
 *
 * where p_max is the probability of the most likely sample value.
 */
#define MIN_ENTROPY_PER_SAMPLE 0.16

/** Length of each entropy sample (in bits) */
#define ENTROPY_SAMPLE_LEN_BITS 12

/**
 * Calculate entropy buffer size
 *
 * @v entropy_bits	Amount of entropy required, in bits
 * @v min_len		Minimum buffer size, in bytes
 * @v max_len		Maximum buffer size, in bytes
 * @ret len		Buffer size, in bytes
 */
static inline __attribute__ (( const, always_inline )) size_t
entropy_bufsize ( unsigned int entropy_bits, size_t min_len, size_t max_len ) {
	unsigned int min_len_bits;
	double min_samples;
	double samples;
	unsigned int samples_int;
	unsigned int len_bits;
	size_t len;

	/* Sanity check */
	linker_assert ( MIN_ENTROPY_PER_SAMPLE <= ENTROPY_SAMPLE_LEN_BITS,
			min_entropy_per_sample_is_impossibly_high );

	/* Calculate number of samples required to contain sufficient entropy */
	samples = ( ( entropy_bits * 1.0 ) / MIN_ENTROPY_PER_SAMPLE );

	/* Increase to minimum length if necessary */
	min_len_bits = ( min_len * 8 );
	min_samples = ( ( min_len_bits * 1.0 ) / ENTROPY_SAMPLE_LEN_BITS );
	if ( samples < min_samples )
		samples = min_samples;

	/* Round up to a whole number of samples.  We don't have the
	 * ceil() function available, so do the rounding by hand.
	 */
	samples_int = samples;
	if ( samples_int < samples )
		samples_int++;
	assert ( samples_int >= samples );

	/* Calculate buffer length in bits */
	len_bits = ( samples_int * ENTROPY_SAMPLE_LEN_BITS );

	/* Calculate buffer length in bytes (rounding up) */
	len = ( ( len_bits + 7 ) / 8 );

	/* Check that buffer is within allowed lengths */
	linker_assert ( len >= min_len, entropy_bufsize_too_short );
	linker_assert ( len <= max_len, entropy_bufsize_too_long );

	/* Floating-point operations are not allowed in iPXE since we
	 * never set up a suitable environment.  Abort the build
	 * unless the calculated length is a compile-time constant.
	 */
	linker_assert ( __builtin_constant_p ( len ),
			entropy_bufsize_not_constant );

	return len;
}

extern int get_entropy_input ( unsigned int entropy_bits, void *data,
			       size_t min_len, size_t max_len );

#endif /* _IPXE_ENTROPY_H */
