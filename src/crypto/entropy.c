/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * Entropy source
 *
 * This algorithm is designed to comply with ANS X9.82 Part 4 (April
 * 2011 Draft) Section 13.3.  This standard is unfortunately not
 * freely available.
 */

#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <ipxe/crypto.h>
#include <ipxe/hash_df.h>
#include <ipxe/entropy.h>

/* Disambiguate the various error causes */
#define EPIPE_REPETITION_COUNT_TEST \
	__einfo_error ( EINFO_EPIPE_REPETITION_COUNT_TEST )
#define EINFO_EPIPE_REPETITION_COUNT_TEST \
	__einfo_uniqify ( EINFO_EPIPE, 0x01, "Repetition count test failed" )
#define EPIPE_ADAPTIVE_PROPORTION_TEST \
	__einfo_error ( EINFO_EPIPE_ADAPTIVE_PROPORTION_TEST )
#define EINFO_EPIPE_ADAPTIVE_PROPORTION_TEST \
	__einfo_uniqify ( EINFO_EPIPE, 0x02, "Adaptive proportion test failed" )

/** Current entropy source */
static struct entropy_source *entropy_source;

/**
 * Enable entropy gathering
 *
 * @ret rc		Return status code
 */
int entropy_enable ( void ) {
	int rc;

	/* Enable selected source, if applicable */
	if ( entropy_source ) {

		/* Enable entropy source */
		if ( ( rc = entropy_source->enable() ) != 0 ) {
			DBGC ( &entropy_source, "ENTROPY could not enable "
			       "source \"%s\": %s\n", entropy_source->name,
			       strerror ( rc ) );
			return rc;
		}

		/* Sanity checks */
		assert ( entropy_source->min_entropy_per_sample > 0 );
		assert ( entropy_source->repetition_count_cutoff > 0 );
		assert ( entropy_source->adaptive_proportion_cutoff > 0 );
		assert ( entropy_source->startup_test_count > 0 );

		return 0;
	}

	/* Find the first working source */
	rc = -ENOENT;
	for_each_table_entry ( entropy_source, ENTROPY_SOURCES ) {
		if ( ( rc = entropy_enable() ) == 0 ) {
			DBGC ( &entropy_source, "ENTROPY using source \"%s\"\n",
			       entropy_source->name );
			break;
		}
	}
	return rc;
}

/**
 * Disable entropy gathering
 *
 */
void entropy_disable ( void ) {

	/* Sanity check */
	assert ( entropy_source != NULL );

	/* Disable entropy gathering, if applicable */
	if ( entropy_source->disable )
		entropy_source->disable();
}

/**
 * Perform repetition count test
 *
 * @v sample		Noise sample
 * @ret rc		Return status code
 *
 * This is the Repetition Count Test defined in ANS X9.82 Part 2
 * (October 2011 Draft) Section 8.5.2.1.2.
 */
static int repetition_count_test ( noise_sample_t sample ) {
	static noise_sample_t most_recent_sample;
	static unsigned int repetition_count = 0;
	unsigned int repetition_count_cutoff =
		entropy_source->repetition_count_cutoff;

	/* A = the most recently seen sample value
	 * B = the number of times that value A has been seen in a row
	 * C = the cutoff value above which the repetition test should fail
	 */

	/* 1.  For each new sample processed:
	 *
	 * (Note that the test for "repetition_count > 0" ensures that
	 * the initial value of most_recent_sample is treated as being
	 * undefined.)
	 */
	if ( ( sample == most_recent_sample ) && ( repetition_count > 0 ) ) {

		/* a) If the new sample = A, then B is incremented by one. */
		repetition_count++;

		/*    i.  If B >= C, then an error condition is raised
		 *        due to a failure of the test
		 */
		if ( repetition_count >= repetition_count_cutoff )
			return -EPIPE_REPETITION_COUNT_TEST;

	} else {

		/* b) Else:
		 *    i.  A = new sample
		 */
		most_recent_sample = sample;

		/*    ii. B = 1 */
		repetition_count = 1;
	}

	return 0;
}

/**
 * Perform adaptive proportion test
 *
 * @v sample		Noise sample
 * @ret rc		Return status code
 *
 * This is the Adaptive Proportion Test for the Most Common Value
 * defined in ANS X9.82 Part 2 (October 2011 Draft) Section 8.5.2.1.3.
 */
static int adaptive_proportion_test ( noise_sample_t sample ) {
	static noise_sample_t current_counted_sample;
	static unsigned int sample_count = ADAPTIVE_PROPORTION_WINDOW_SIZE;
	static unsigned int repetition_count;
	unsigned int adaptive_proportion_cutoff =
		entropy_source->adaptive_proportion_cutoff;

	/* A = the sample value currently being counted
	 * B = the number of samples examined in this run of the test so far
	 * N = the total number of samples that must be observed in
	 *     one run of the test, also known as the "window size" of
	 *     the test
	 * B = the current number of times that S (sic) has been seen
	 *     in the W (sic) samples examined so far
	 * C = the cutoff value above which the repetition test should fail
	 * W = the probability of a false positive: 2^-30
	 */

	/* 1.  The entropy source draws the current sample from the
	 *     noise source.
	 *
	 * (Nothing to do; we already have the current sample.)
	 */

	/* 2.  If S = N, then a new run of the test begins: */
	if ( sample_count == ADAPTIVE_PROPORTION_WINDOW_SIZE ) {

		/* a.  A = the current sample */
		current_counted_sample = sample;

		/* b.  S = 0 */
		sample_count = 0;

		/* c. B = 0 */
		repetition_count = 0;

	} else {

		/* Else: (the test is already running)
		 * a.  S = S + 1
		 */
		sample_count++;

		/* b.  If A = the current sample, then: */
		if ( sample == current_counted_sample ) {

			/* i.   B = B + 1 */
			repetition_count++;

			/* ii.  If S (sic) > C then raise an error
			 *      condition, because the test has
			 *      detected a failure
			 */
			if ( repetition_count > adaptive_proportion_cutoff )
				return -EPIPE_ADAPTIVE_PROPORTION_TEST;

		}
	}

	return 0;
}

/**
 * Get noise sample
 *
 * @ret noise		Noise sample
 * @ret rc		Return status code
 *
 * This is the GetNoise function defined in ANS X9.82 Part 2
 * (October 2011 Draft) Section 6.5.2.
 */
int get_noise ( noise_sample_t *noise ) {

	/* Sanity check */
	assert ( entropy_source != NULL );

	return entropy_source->get_noise ( noise );
}

/**
 * Get entropy sample
 *
 * @ret entropy		Entropy sample
 * @ret rc		Return status code
 *
 * This is the GetEntropy function defined in ANS X9.82 Part 2
 * (October 2011 Draft) Section 6.5.1.
 */
static int get_entropy ( entropy_sample_t *entropy ) {
	static int rc = 0;
	noise_sample_t noise;

	/* Sanity check */
	assert ( entropy_source != NULL );

	/* Any failure is permanent */
	if ( rc != 0 )
		return rc;

	/* Get noise sample */
	if ( ( rc = get_noise ( &noise ) ) != 0 )
		return rc;

	/* Perform Repetition Count Test and Adaptive Proportion Test
	 * as mandated by ANS X9.82 Part 2 (October 2011 Draft)
	 * Section 8.5.2.1.1.
	 */
	if ( ( rc = repetition_count_test ( noise ) ) != 0 )
		return rc;
	if ( ( rc = adaptive_proportion_test ( noise ) ) != 0 )
		return rc;

	/* We do not use any optional conditioning component */
	*entropy = noise;

	return 0;
}

/**
 * Create next nonce value
 *
 * @ret nonce		Nonce
 *
 * This is the MakeNextNonce function defined in ANS X9.82 Part 4
 * (April 2011 Draft) Section 13.3.4.2.
 */
static uint32_t make_next_nonce ( void ) {
	static uint32_t nonce;

	/* The simplest implementation of a nonce uses a large counter */
	nonce++;

	return nonce;
}

/**
 * Obtain entropy input temporary buffer
 *
 * @v min_entropy	Min-entropy required
 * @v tmp		Temporary buffer
 * @v tmp_len		Length of temporary buffer
 * @ret rc		Return status code
 *
 * This is (part of) the implementation of the Get_entropy_input
 * function (using an entropy source as the source of entropy input
 * and condensing each entropy source output after each GetEntropy
 * call) as defined in ANS X9.82 Part 4 (April 2011 Draft) Section
 * 13.3.4.2.
 */
int get_entropy_input_tmp ( min_entropy_t min_entropy, uint8_t *tmp,
			    size_t tmp_len ) {
	static unsigned int startup_tested = 0;
	struct {
		uint32_t nonce;
		entropy_sample_t sample;
	} __attribute__ (( packed )) data;;
	uint8_t df_buf[tmp_len];
	min_entropy_t entropy_total;
	unsigned int num_samples;
	unsigned int i;
	int rc;

	/* Enable entropy gathering */
	if ( ( rc = entropy_enable() ) != 0 )
		return rc;

	/* Perform mandatory startup tests, if not yet performed */
	for ( ; startup_tested < entropy_source->startup_test_count ;
	      startup_tested++ ) {
		if ( ( rc = get_entropy ( &data.sample ) ) != 0 )
			goto err_get_entropy;
	}

	/* 3.  entropy_total = 0 */
	entropy_total = MIN_ENTROPY ( 0 );

	/* 4.  tmp = a fixed n-bit value, such as 0^n */
	memset ( tmp, 0, tmp_len );

	/* 5.  While ( entropy_total < min_entropy ) */
	for ( num_samples = 0 ; entropy_total < min_entropy ; num_samples++ ) {
		/* 5.1.  ( status, entropy_bitstring, assessed_entropy )
		 *       = GetEntropy()
		 * 5.2.  If status indicates an error, return ( status, Null )
		 */
		if ( ( rc = get_entropy ( &data.sample ) ) != 0 )
			goto err_get_entropy;

		/* 5.3.  nonce = MakeNextNonce() */
		data.nonce = make_next_nonce();

		/* 5.4.  tmp = tmp XOR
		 *             df ( ( nonce || entropy_bitstring ), n )
		 */
		hash_df ( &entropy_hash_df_algorithm, &data, sizeof ( data ),
			  df_buf, sizeof ( df_buf ) );
		for ( i = 0 ; i < tmp_len ; i++ )
			tmp[i] ^= df_buf[i];

		/* 5.5.  entropy_total = entropy_total + assessed_entropy */
		entropy_total += entropy_source->min_entropy_per_sample;
	}

	/* Disable entropy gathering */
	entropy_disable();

	DBGC ( &entropy_source, "ENTROPY gathered %d bits in %d samples\n",
	       ( min_entropy / MIN_ENTROPY_SCALE ), num_samples );
	return 0;

 err_get_entropy:
	entropy_disable();
	return rc;
}

/* Drag in objects via entropy_enable */
REQUIRING_SYMBOL ( entropy_enable );

/* Drag in entropy configuration */
REQUIRE_OBJECT ( config_entropy );
