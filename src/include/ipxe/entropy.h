#ifndef _IPXE_ENTROPY_H
#define _IPXE_ENTROPY_H

/** @file
 *
 * Entropy source
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <ipxe/hash_df.h>
#include <ipxe/sha256.h>
#include <ipxe/tables.h>
#include <config/entropy.h>

/** A noise sample */
typedef uint8_t noise_sample_t;

/** An entropy sample */
typedef uint8_t entropy_sample_t;

/** An amount of min-entropy
 *
 * Expressed as a fixed-point quantity in order to avoid floating
 * point calculations.
 */
typedef unsigned int min_entropy_t;

/** Fixed-point scale for min-entropy amounts */
#define MIN_ENTROPY_SCALE ( 1 << 16 )

/**
 * Construct a min-entropy fixed-point value
 *
 * @v bits		min-entropy in bits
 * @ret min_entropy	min-entropy as a fixed-point value
 */
#define MIN_ENTROPY( bits ) \
	( ( min_entropy_t ) ( (bits) * MIN_ENTROPY_SCALE ) )

/**
 * Repetition count test state
 *
 * This is the state for the repetition Count Test defined in ANS
 * X9.82 Part 2 (October 2011 Draft) Section 8.5.2.1.2.
 */
struct entropy_repetition_count_test {
	/**
	 * A = the most recently seen sample value
	 */
	noise_sample_t most_recent_sample;
	/**
	 * B = the number of times that value A has been seen in a row
	 */
	unsigned int repetition_count;
	/**
	 * C = the cutoff value above which the repetition test should fail
	 *
	 * Filled in by entropy_init().
	 */
	unsigned int cutoff;
};

/**
 * Adaptive proportion test state
 *
 * This is the state for the Adaptive Proportion Test for the Most
 * Common Value defined in ANS X9.82 Part 2 (October 2011 Draft)
 * Section 8.5.2.1.3.
 */
struct entropy_adaptive_proportion_test {
	/**
	 * A = the sample value currently being counted
	 */
	noise_sample_t current_counted_sample;
	/**
	 * S = the number of samples examined in this run of the test so far
	 */
	unsigned int sample_count;
	/**
	 * B = the current number of times that S (sic) has been seen
	 *     in the W (sic) samples examined so far
	 */
	unsigned int repetition_count;
	/**
	 * C = the cutoff value above which the repetition test should fail
	 *
	 * Filled in by entropy_init().
	 */
	unsigned int cutoff;
};

/**
 * Startup test state
 *
 * ANS X9.82 Part 2 (October 2011 Draft) Section 8.5.2.1.5 requires
 * that at least one full cycle of the continuous tests must be
 * performed at start-up.
 */
struct entropy_startup_test {
	/** Number of startup tests performed */
	unsigned int tested;
	/**
	 * Number of startup tests required for one full cycle
	 *
	 * Filled in by entropy_init().
	 */
	unsigned int count;
};

/** An entropy source */
struct entropy_source {
	/** Name */
	const char *name;
	/**
	 * min-entropy per sample
	 *
	 * min-entropy is defined in ANS X9.82 Part 1-2006 Section 8.3 and in
	 * NIST SP 800-90 Appendix C.3 as
	 *
	 *    H_min = -log2 ( p_max )
	 *
	 * where p_max is the probability of the most likely sample value.
	 *
	 * Filled in by entropy_init().
	 */
	min_entropy_t min_entropy_per_sample;
	/** Repetition count test state */
	struct entropy_repetition_count_test repetition_count_test;
	/** Adaptive proportion test state */
	struct entropy_adaptive_proportion_test adaptive_proportion_test;
	/** Startup test state */
	struct entropy_startup_test startup_test;
	/**
	 * Failure status (if any)
	 *
	 * Any failure of an entropy source is regarded as permanent.
	 */
	int rc;

	/**
	 * Enable entropy gathering
	 *
	 * @ret rc		Return status code
	 */
	int ( * enable ) ( void );
	/**
	 * Disable entropy gathering
	 *
	 */
	void ( * disable ) ( void );
	/**
	 * Get noise sample
	 *
	 * @ret noise		Noise sample
	 * @ret rc		Return status code
	 *
	 * This is the GetNoise function defined in ANS X9.82 Part 2
	 * (October 2011 Draft) Section 6.5.2.
	 */
	int ( * get_noise ) ( noise_sample_t *noise );
};

/** Entropy source table */
#define ENTROPY_SOURCES __table ( struct entropy_source, "entropy_sources" )

/** Declare an entropy source */
#define __entropy_source( order ) __table_entry ( ENTROPY_SOURCES, order )

/** @defgroup entropy_source_order Entropy source order
 *
 * @{
 */

#define ENTROPY_PREFERRED	01	/**< Preferred entropy source */
#define ENTROPY_NORMAL		02	/**< Normal entropy source */
#define ENTROPY_FALLBACK	03	/**< Fallback entropy source */

/** @} */

extern int get_entropy_input_tmp ( min_entropy_t min_entropy, uint8_t *tmp,
				   size_t tmp_len );

/** Use SHA-256 as the underlying hash algorithm for Hash_df
 *
 * Hash_df using SHA-256 is an Approved algorithm in ANS X9.82.
 */
#define entropy_hash_df_algorithm sha256_algorithm

/** Underlying hash algorithm output length (in bytes) */
#define ENTROPY_HASH_DF_OUTLEN_BYTES SHA256_DIGEST_SIZE

/**
 * Get noise sample
 *
 * @v source		Entropy source
 * @ret noise		Noise sample
 * @ret rc		Return status code
 *
 * This is the GetNoise function defined in ANS X9.82 Part 2
 * (October 2011 Draft) Section 6.5.2.
 */
static inline __attribute__ (( always_inline )) int
get_noise ( struct entropy_source *source, noise_sample_t *noise ) {

	return source->get_noise ( noise );
}

/**
 * Obtain entropy input
 *
 * @v min_entropy_bits	Minimum amount of entropy, in bits
 * @v data		Data buffer
 * @v min_len		Minimum length of entropy input, in bytes
 * @v max_len		Maximum length of entropy input, in bytes
 * @ret len		Length of entropy input, in bytes, or negative error
 *
 * This is the implementation of the Get_entropy_input function (using
 * an entropy source as the source of entropy input and condensing
 * each entropy source output after each GetEntropy call) as defined
 * in ANS X9.82 Part 4 (April 2011 Draft) Section 13.3.4.2.
 *
 * This function is inlined since the entropy amount and length inputs
 * are always compile-time constants.
 */
static inline __attribute__ (( always_inline )) int
get_entropy_input ( unsigned int min_entropy_bits, void *data, size_t min_len,
		    size_t max_len ) {
	size_t tmp_len = ( ( ( min_entropy_bits * 2 ) + 7 ) / 8 );
	uint8_t tmp_buf[ tmp_len ];
	uint8_t *tmp = ( ( tmp_len > max_len ) ? tmp_buf : data );
	unsigned int n;
	int rc;

	/* Sanity check */
	build_assert ( min_entropy_bits <= ( 8 * max_len ) );

	/* Round up minimum entropy to an integral number of bytes */
	min_entropy_bits = ( ( min_entropy_bits + 7 ) & ~7 );

	/* (Unnumbered).  The output length of the hash function shall
	 * meet or exceed the security strength indicated by the
	 * min_entropy parameter.
	 */
	build_assert ( ( 8 * ENTROPY_HASH_DF_OUTLEN_BYTES ) >=
		       min_entropy_bits );

	/* 1.  If ( min_length > max_length ), then return ( FAILURE, Null ) */
	build_assert ( min_len <= max_len );

	/* 2.  n = 2 * min_entropy */
	n = ( 2 * min_entropy_bits );

	/* 3.  entropy_total = 0
	 * 4.  tmp = a fixed n-bit value, such as 0^n
	 * 5.  While ( entropy_total < min_entropy )
	 *     5.1.  ( status, entropy_bitstring, assessed_entropy )
	 *           = GetEntropy()
	 *     5.2.  If status indicates an error, return ( status, Null )
	 *     5.3.  nonce = MakeNextNonce()
	 *     5.4.  tmp = tmp XOR df ( ( nonce || entropy_bitstring ), n )
	 *     5.5.  entropy_total = entropy_total + assessed_entropy
	 *
	 * (The implementation of these steps is inside the function
	 * get_entropy_input_tmp().)
	 */
	build_assert ( __builtin_constant_p ( tmp_len ) );
	build_assert ( n == ( 8 * tmp_len ) );
	if ( ( rc = get_entropy_input_tmp ( MIN_ENTROPY ( min_entropy_bits ),
					    tmp, tmp_len ) ) != 0 ) {
		return rc;
	}

	/* 6.  If ( n < min_length ), then tmp = tmp || 0^(min_length-n)
	 * 7.  If ( n > max_length ), then tmp = df ( tmp, max_length )
	 * 8.  Return ( SUCCESS, tmp )
	 */
	if ( tmp_len < min_len ) {
		/* (Data is already in-place.) */
		build_assert ( data == tmp );
		memset ( ( data + tmp_len ), 0, ( min_len - tmp_len ) );
		return min_len;
	} else if ( tmp_len > max_len ) {
		build_assert ( tmp == tmp_buf );
		hash_df ( &entropy_hash_df_algorithm, tmp, tmp_len,
			  data, max_len );
		return max_len;
	} else {
		/* (Data is already in-place.) */
		build_assert ( data == tmp );
		return tmp_len;
	}
}

/**
 * Calculate cutoff value for the repetition count test
 *
 * @v min_entropy_per_sample	Min-entropy per sample
 * @ret cutoff			Cutoff value
 *
 * This is the cutoff value for the Repetition Count Test defined in
 * ANS X9.82 Part 2 (October 2011 Draft) Section 8.5.2.1.2.
 */
static inline __attribute__ (( always_inline )) unsigned int
entropy_repetition_count_cutoff ( min_entropy_t min_entropy_per_sample ) {
	double max_repetitions;
	unsigned int cutoff;

	/* The cutoff formula for the repetition test is:
	 *
	 *   C = ( 1 + ( -log2(W) / H_min ) )
	 *
	 * where W is set at 2^(-30) (in ANS X9.82 Part 2 (October
	 * 2011 Draft) Section 8.5.2.1.3.1).
	 */
	max_repetitions = ( 1 + ( MIN_ENTROPY ( 30 ) /
				  min_entropy_per_sample ) );

	/* Round up to a whole number of repetitions.  We don't have
	 * the ceil() function available, so do the rounding by hand.
	 */
	cutoff = max_repetitions;
	if ( cutoff < max_repetitions )
		cutoff++;
	build_assert ( cutoff >= max_repetitions );

	/* Floating-point operations are not allowed in iPXE since we
	 * never set up a suitable environment.  Abort the build
	 * unless the calculated number of repetitions is a
	 * compile-time constant.
	 */
	build_assert ( __builtin_constant_p ( cutoff ) );

	return cutoff;
}

/**
 * Window size for the adaptive proportion test
 *
 * ANS X9.82 Part 2 (October 2011 Draft) Section 8.5.2.1.3.1.1 allows
 * five possible window sizes: 16, 64, 256, 4096 and 65536.
 *
 * We expect to generate relatively few (<256) entropy samples during
 * a typical iPXE run; the use of a large window size would mean that
 * the test would never complete a single cycle.  We use a window size
 * of 64, which is the smallest window size that permits values of
 * H_min down to one bit per sample.
 */
#define ADAPTIVE_PROPORTION_WINDOW_SIZE 64

/**
 * Combine adaptive proportion test window size and min-entropy
 *
 * @v n			N (window size)
 * @v h			H (min-entropy)
 * @ret n_h		(N,H) combined value
 */
#define APC_N_H( n, h ) ( ( (n) << 8 ) | (h) )

/**
 * Define a row of the adaptive proportion cutoff table
 *
 * @v h			H (min-entropy)
 * @v c16		Cutoff for N=16
 * @v c64		Cutoff for N=64
 * @v c256		Cutoff for N=256
 * @v c4096		Cutoff for N=4096
 * @v c65536		Cutoff for N=65536
 */
#define APC_TABLE_ROW( h, c16, c64, c256, c4096, c65536)	   \
	case APC_N_H ( 16, h ) :	return c16;		   \
	case APC_N_H ( 64, h ) :	return c64;   		   \
	case APC_N_H ( 256, h ) :	return c256;		   \
	case APC_N_H ( 4096, h ) :	return c4096;		   \
	case APC_N_H ( 65536, h ) :	return c65536;

/** Value used to represent "N/A" in adaptive proportion cutoff table */
#define APC_NA 0

/**
 * Look up value in adaptive proportion test cutoff table
 *
 * @v n			N (window size)
 * @v h			H (min-entropy)
 * @ret cutoff		Cutoff
 *
 * This is the table of cutoff values defined in ANS X9.82 Part 2
 * (October 2011 Draft) Section 8.5.2.1.3.1.2.
 */
static inline __attribute__ (( always_inline )) unsigned int
entropy_adaptive_proportion_cutoff_lookup ( unsigned int n, unsigned int h ) {
	switch ( APC_N_H ( n, h ) ) {
		APC_TABLE_ROW (  1, APC_NA,     51,    168,   2240,  33537 );
		APC_TABLE_ROW (  2, APC_NA,     35,    100,   1193,  17053 );
		APC_TABLE_ROW (  3,     10,     24,     61,    643,   8705 );
		APC_TABLE_ROW (  4,      8,     16,     38,    354,   4473 );
		APC_TABLE_ROW (  5,      6,     12,     25,    200,   2321 );
		APC_TABLE_ROW (  6,      5,      9,     17,    117,   1220 );
		APC_TABLE_ROW (  7,      4,      7,     15,     71,    653 );
		APC_TABLE_ROW (  8,      4,      5,      9,     45,    358 );
		APC_TABLE_ROW (  9,      3,      4,      7,     30,    202 );
		APC_TABLE_ROW ( 10,      3,      4,      5,     21,    118 );
		APC_TABLE_ROW ( 11,      2,      3,      4,     15,     71 );
		APC_TABLE_ROW ( 12,      2,      3,      4,     11,     45 );
		APC_TABLE_ROW ( 13,      2,      2,      3,      9,     30 );
		APC_TABLE_ROW ( 14,      2,      2,      3,      7,     21 );
		APC_TABLE_ROW ( 15,      1,      2,      2,      6,     15 );
		APC_TABLE_ROW ( 16,      1,      2,      2,      5,     11 );
		APC_TABLE_ROW ( 17,      1,      1,      2,      4,      9 );
		APC_TABLE_ROW ( 18,      1,      1,      2,      4,      7 );
		APC_TABLE_ROW ( 19,      1,      1,      1,      3,      6 );
		APC_TABLE_ROW ( 20,      1,      1,      1,      3,      5 );
	default:
		return APC_NA;
	}
}

/**
 * Calculate cutoff value for the adaptive proportion test
 *
 * @v min_entropy_per_sample	Min-entropy per sample
 * @ret cutoff			Cutoff value
 *
 * This is the cutoff value for the Adaptive Proportion Test defined
 * in ANS X9.82 Part 2 (October 2011 Draft) Section 8.5.2.1.3.1.2.
 */
static inline __attribute__ (( always_inline )) unsigned int
entropy_adaptive_proportion_cutoff ( min_entropy_t min_entropy_per_sample ) {
	unsigned int h;
	unsigned int n;
	unsigned int cutoff;

	/* Look up cutoff value in cutoff table */
	n = ADAPTIVE_PROPORTION_WINDOW_SIZE;
	h = ( min_entropy_per_sample / MIN_ENTROPY_SCALE );
	cutoff = entropy_adaptive_proportion_cutoff_lookup ( n, h );

	/* Fail unless cutoff value is a compile-time constant */
	build_assert ( __builtin_constant_p ( cutoff ) );

	/* Fail if cutoff value is N/A */
	build_assert ( cutoff != APC_NA );

	return cutoff;
}

/**
 * Calculate number of samples required for startup tests
 *
 * @v repetition_count_cutoff	Repetition count test cutoff value
 * @v adaptive_proportion_cutoff Adaptive proportion test cutoff value
 * @ret num_samples		Number of samples required
 *
 * ANS X9.82 Part 2 (October 2011 Draft) Section 8.5.2.1.5 requires
 * that at least one full cycle of the continuous tests must be
 * performed at start-up.
 */
static inline __attribute__ (( always_inline )) unsigned int
entropy_startup_test_count ( unsigned int repetition_count_cutoff,
			     unsigned int adaptive_proportion_cutoff ) {
	unsigned int num_samples;

	/* At least max(N,C) samples shall be generated by the noise
	 * source for start-up testing.
	 */
	num_samples = repetition_count_cutoff;
	if ( num_samples < adaptive_proportion_cutoff )
		num_samples = adaptive_proportion_cutoff;
	build_assert ( __builtin_constant_p ( num_samples ) );

	return num_samples;
}

/**
 * Initialise entropy source
 *
 * @v source			Entropy source
 * @v min_entropy_per_sample	Min-entropy per sample
 *
 * The cutoff value calculations for the repetition count test and the
 * adaptive proportion test are provided as static inline functions
 * since the results will always be compile-time constants.
 */
static inline __attribute__ (( always_inline )) void
entropy_init ( struct entropy_source *source,
	       min_entropy_t min_entropy_per_sample ) {
	unsigned int repetition_count_cutoff;
	unsigned int adaptive_proportion_cutoff;
	unsigned int startup_test_count;

	/* Sanity check */
	build_assert ( min_entropy_per_sample > MIN_ENTROPY ( 0 ) );
	build_assert ( min_entropy_per_sample <=
		       MIN_ENTROPY ( 8 * sizeof ( noise_sample_t ) ) );

	/* Calculate test cutoff values */
	repetition_count_cutoff =
		entropy_repetition_count_cutoff ( min_entropy_per_sample );
	adaptive_proportion_cutoff =
		entropy_adaptive_proportion_cutoff ( min_entropy_per_sample );
	startup_test_count =
		entropy_startup_test_count ( repetition_count_cutoff,
					     adaptive_proportion_cutoff );

	/* Record min-entropy per sample and test cutoff values */
	source->min_entropy_per_sample = min_entropy_per_sample;
	source->repetition_count_test.cutoff = repetition_count_cutoff;
	source->adaptive_proportion_test.cutoff = adaptive_proportion_cutoff;
	source->startup_test.count = startup_test_count;
}

extern int entropy_enable ( struct entropy_source *source );
extern void entropy_disable ( struct entropy_source *source );
extern int get_noise ( struct entropy_source *source, noise_sample_t *noise );

#endif /* _IPXE_ENTROPY_H */
