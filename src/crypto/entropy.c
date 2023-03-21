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

/**
 * Initialise repetition count test
 *
 * @v source		Entropy source
 */
static void repetition_count_test_init ( struct entropy_source *source ) {
	struct entropy_repetition_count_test *test =
		&source->repetition_count_test;

	/* Sanity checks */
	assert ( test->repetition_count == 0 );
	assert ( test->cutoff > 0 );
}

/**
 * Perform repetition count test
 *
 * @v source		Entropy source
 * @v sample		Noise sample
 * @ret rc		Return status code
 *
 * This is the Repetition Count Test defined in ANS X9.82 Part 2
 * (October 2011 Draft) Section 8.5.2.1.2.
 */
static int repetition_count_test ( struct entropy_source *source,
				   noise_sample_t sample ) {
	struct entropy_repetition_count_test *test =
		&source->repetition_count_test;

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
	if ( ( sample == test->most_recent_sample ) &&
	     ( test->repetition_count > 0 ) ) {

		/* a) If the new sample = A, then B is incremented by one. */
		test->repetition_count++;

		/*    i.  If B >= C, then an error condition is raised
		 *        due to a failure of the test
		 */
		if ( test->repetition_count >= test->cutoff ) {
			DBGC ( source, "ENTROPY %s excessively repeated "
			       "value %d (%d/%d)\n", source->name, sample,
			       test->repetition_count, test->cutoff );
			return -EPIPE_REPETITION_COUNT_TEST;
		}

	} else {

		/* b) Else:
		 *    i.  A = new sample
		 */
		test->most_recent_sample = sample;

		/*    ii. B = 1 */
		test->repetition_count = 1;
	}

	return 0;
}

/**
 * Initialise adaptive proportion test
 *
 * @v source		Entropy source
 */
static void adaptive_proportion_test_init ( struct entropy_source *source ) {
	struct entropy_adaptive_proportion_test *test =
		&source->adaptive_proportion_test;

	/* Sanity checks */
	assert ( test->sample_count == 0 );
	assert ( test->repetition_count == 0 );
	assert ( test->cutoff > 0 );

	/* Ensure that a new test run starts immediately */
	test->sample_count = ADAPTIVE_PROPORTION_WINDOW_SIZE;
}

/**
 * Perform adaptive proportion test
 *
 * @v source		Entropy source
 * @v sample		Noise sample
 * @ret rc		Return status code
 *
 * This is the Adaptive Proportion Test for the Most Common Value
 * defined in ANS X9.82 Part 2 (October 2011 Draft) Section 8.5.2.1.3.
 */
static int adaptive_proportion_test ( struct entropy_source *source,
				      noise_sample_t sample ) {
	struct entropy_adaptive_proportion_test *test =
		&source->adaptive_proportion_test;

	/* A = the sample value currently being counted
	 * S = the number of samples examined in this run of the test so far
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
	if ( test->sample_count == ADAPTIVE_PROPORTION_WINDOW_SIZE ) {

		/* a.  A = the current sample */
		test->current_counted_sample = sample;

		/* b.  S = 0 */
		test->sample_count = 0;

		/* c. B = 0 */
		test->repetition_count = 0;

	} else {

		/* Else: (the test is already running)
		 * a.  S = S + 1
		 */
		test->sample_count++;

		/* b.  If A = the current sample, then: */
		if ( sample == test->current_counted_sample ) {

			/* i.   B = B + 1 */
			test->repetition_count++;

			/* ii.  If S (sic) > C then raise an error
			 *      condition, because the test has
			 *      detected a failure
			 */
			if ( test->repetition_count > test->cutoff ) {
				DBGC ( source, "ENTROPY %s excessively "
				       "repeated value %d (%d/%d)\n",
				       source->name, sample,
				       test->repetition_count, test->cutoff );
				return -EPIPE_ADAPTIVE_PROPORTION_TEST;
			}
		}
	}

	return 0;
}

/**
 * Get entropy sample
 *
 * @v source		Entropy source
 * @ret entropy		Entropy sample
 * @ret rc		Return status code
 *
 * This is the GetEntropy function defined in ANS X9.82 Part 2
 * (October 2011 Draft) Section 6.5.1.
 */
static int get_entropy ( struct entropy_source *source,
			 entropy_sample_t *entropy ) {
	noise_sample_t noise;
	int rc;

	/* Any failure is permanent */
	if ( ( rc = source->rc ) != 0 )
		goto err_broken;

	/* Get noise sample */
	if ( ( rc = get_noise ( source, &noise ) ) != 0 )
		goto err_get_noise;

	/* Perform Repetition Count Test and Adaptive Proportion Test
	 * as mandated by ANS X9.82 Part 2 (October 2011 Draft)
	 * Section 8.5.2.1.1.
	 */
	if ( ( rc = repetition_count_test ( source, noise ) ) != 0 )
		goto err_repetition_count_test;
	if ( ( rc = adaptive_proportion_test ( source, noise ) ) != 0 )
		goto err_adaptive_proportion_test;

	/* We do not use any optional conditioning component */
	*entropy = noise;

	return 0;

 err_adaptive_proportion_test:
 err_repetition_count_test:
 err_get_noise:
	source->rc = rc;
 err_broken:
	return rc;
}

/**
 * Initialise startup test
 *
 * @v source		Entropy source
 */
static void startup_test_init ( struct entropy_source *source ) {
	struct entropy_startup_test *test = &source->startup_test;

	/* Sanity check */
	assert ( test->tested == 0 );
	assert ( test->count > 0 );
}

/**
 * Perform startup test
 *
 * @v source		Entropy source
 * @ret rc		Return status code
 */
static int startup_test ( struct entropy_source *source ) {
	struct entropy_startup_test *test = &source->startup_test;
	entropy_sample_t sample;
	int rc;

	/* Perform mandatory number of startup tests */
	for ( ; test->tested < test->count ; test->tested++ ) {
		if ( ( rc = get_entropy ( source, &sample ) ) != 0 ) {
			DBGC ( source, "ENTROPY %s failed: %s\n",
			       source->name, strerror ( rc ) );
			return rc;
		}
	}

	return 0;
}

/**
 * Enable entropy gathering
 *
 * @v source		Entropy source
 * @ret rc		Return status code
 */
int entropy_enable ( struct entropy_source *source ) {
	int rc;

	/* Refuse to enable a previously failed source */
	if ( ( rc = source->rc ) != 0 )
		return rc;

	/* Enable entropy source */
	if ( ( rc = source->enable() ) != 0 ) {
		DBGC ( source, "ENTROPY %s could not enable: %s\n",
		       source->name, strerror ( rc ) );
		source->rc = rc;
		return rc;
	}

	/* Sanity check */
	assert ( source->min_entropy_per_sample > 0 );

	/* Initialise test state if this source has not previously been used */
	if ( source->startup_test.tested == 0 ) {
		repetition_count_test_init ( source );
		adaptive_proportion_test_init ( source );
		startup_test_init ( source );
	}

	DBGC ( source, "ENTROPY %s enabled\n", source->name );
	return 0;
}

/**
 * Enable and test entropy source
 *
 * @v source		Entropy source
 * @ret rc		Return status code
 */
static int entropy_enable_and_test ( struct entropy_source *source ) {
	int rc;

	/* Enable source */
	if ( ( rc = entropy_enable ( source ) ) != 0 )
		goto err_enable;

	/* Test source */
	if ( ( rc = startup_test ( source ) ) != 0 )
		goto err_test;

	DBGC ( source, "ENTROPY %s passed %d startup tests\n",
	       source->name, source->startup_test.count );
	return 0;

 err_test:
	entropy_disable ( source );
 err_enable:
	assert ( source->rc == rc );
	return rc;
}

/**
 * Enable first working entropy source
 *
 * @v source		Entropy source to fill in
 * @ret rc		Return status code
 */
static int entropy_enable_working ( struct entropy_source **source ) {
	int rc;

	/* Find the first working source */
	rc = -ENOENT;
	for_each_table_entry ( *source, ENTROPY_SOURCES ) {
		if ( ( rc = entropy_enable_and_test ( *source ) ) == 0 )
			return 0;
	}

	DBGC ( *source, "ENTROPY has no working sources: %s\n",
	       strerror ( rc ) );
	return rc;
}

/**
 * Disable entropy gathering
 *
 * @v source		Entropy source
 */
void entropy_disable ( struct entropy_source *source ) {

	/* Disable entropy gathering, if applicable */
	if ( source->disable )
		source->disable();

	DBGC ( source, "ENTROPY %s disabled\n", source->name );
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
	struct entropy_source *source;
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
	if ( ( rc = entropy_enable_working ( &source ) ) != 0 )
		goto err_enable_working;

	/* Sanity checks */
	assert ( source->startup_test.count > 0 );
	assert ( source->startup_test.tested >= source->startup_test.count );

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
		if ( ( rc = get_entropy ( source, &data.sample ) ) != 0 )
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
		entropy_total += source->min_entropy_per_sample;
	}

	/* Disable entropy gathering */
	entropy_disable ( source );

	DBGC ( source, "ENTROPY %s gathered %d bits in %d samples\n",
	       source->name, ( min_entropy / MIN_ENTROPY_SCALE ), num_samples );
	return 0;

 err_get_entropy:
	entropy_disable ( source );
	assert ( source->rc == rc );
 err_enable_working:
	return rc;
}

/* Drag in objects via entropy_enable */
REQUIRING_SYMBOL ( entropy_enable );

/* Drag in entropy configuration */
REQUIRE_OBJECT ( config_entropy );
