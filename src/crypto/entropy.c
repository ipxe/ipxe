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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

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
#include <ipxe/crypto.h>
#include <ipxe/hash_df.h>
#include <ipxe/entropy.h>

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
	noise_sample_t noise;
	int rc;

	/* Get noise sample */
	if ( ( rc = get_noise ( &noise ) ) != 0 )
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
 * @v num_samples	Number of entropy samples
 * @v tmp		Temporary buffer
 * @v tmp_len		Length of temporary buffer
 * @ret rc		Return status code
 *
 * This is (part of) the implementation of the Get_entropy_input
 * function (using an entropy source as the source of entropy input
 * and condensing each entropy source output after each GetEntropy
 * call) as defined in ANS X9.82 Part 4 (April 2011 Draft) Section
 * 13.3.4.2.
 *
 * To minimise code size, the number of samples required is calculated
 * at compilation time.
 */
int get_entropy_input_tmp ( unsigned int num_samples, uint8_t *tmp,
			    size_t tmp_len ) {
	struct {
		uint32_t nonce;
		entropy_sample_t sample;
	} __attribute__ (( packed )) data;;
	uint8_t df_buf[tmp_len];
	unsigned int i;
	int rc;

	/* Enable entropy gathering */
	entropy_enable();

	/* 3.  entropy_total = 0
	 *
	 * (Nothing to do; the number of entropy samples required has
	 * already been precalculated.)
	 */

	/* 4.  tmp = a fixed n-bit value, such as 0^n */
	memset ( tmp, 0, tmp_len );

	/* 5.  While ( entropy_total < min_entropy ) */
	while ( num_samples-- ) {
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
		hash_df ( &data, sizeof ( data ), df_buf, sizeof ( df_buf ) );
		for ( i = 0 ; i < tmp_len ; i++ )
			tmp[i] ^= df_buf[i];

		/* 5.5.  entropy_total = entropy_total + assessed_entropy
		 *
		 * (Nothing to do; the number of entropy samples
		 * required has already been precalculated.)
		 */
	}

	/* Disable entropy gathering */
	entropy_disable();

	return 0;

 err_get_entropy:
	entropy_disable();
	return rc;
}
