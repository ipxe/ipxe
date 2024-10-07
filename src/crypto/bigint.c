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

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <ipxe/profile.h>
#include <ipxe/bigint.h>

/** @file
 *
 * Big integer support
 */

/** Modular multiplication overall profiler */
static struct profiler bigint_mod_multiply_profiler __profiler =
	{ .name = "bigint_mod_multiply" };

/** Modular multiplication multiply step profiler */
static struct profiler bigint_mod_multiply_multiply_profiler __profiler =
	{ .name = "bigint_mod_multiply.multiply" };

/** Modular multiplication rescale step profiler */
static struct profiler bigint_mod_multiply_rescale_profiler __profiler =
	{ .name = "bigint_mod_multiply.rescale" };

/** Modular multiplication subtract step profiler */
static struct profiler bigint_mod_multiply_subtract_profiler __profiler =
	{ .name = "bigint_mod_multiply.subtract" };

/**
 * Conditionally swap big integers (in constant time)
 *
 * @v first0		Element 0 of big integer to be conditionally swapped
 * @v second0		Element 0 of big integer to be conditionally swapped
 * @v size		Number of elements in big integers
 * @v swap		Swap first and second big integers
 */
void bigint_swap_raw ( bigint_element_t *first0, bigint_element_t *second0,
		       unsigned int size, int swap ) {
	bigint_element_t mask;
	bigint_element_t xor;
	unsigned int i;

	/* Construct mask */
	mask = ( ( bigint_element_t ) ( ! swap ) - 1 );

	/* Conditionally swap elements */
	for ( i = 0 ; i < size ; i++ ) {
		xor = ( mask & ( first0[i] ^ second0[i] ) );
		first0[i] ^= xor;
		second0[i] ^= xor;
	}
}

/**
 * Multiply big integers
 *
 * @v multiplicand0	Element 0 of big integer to be multiplied
 * @v multiplicand_size	Number of elements in multiplicand
 * @v multiplier0	Element 0 of big integer to be multiplied
 * @v multiplier_size	Number of elements in multiplier
 * @v result0		Element 0 of big integer to hold result
 */
void bigint_multiply_raw ( const bigint_element_t *multiplicand0,
			   unsigned int multiplicand_size,
			   const bigint_element_t *multiplier0,
			   unsigned int multiplier_size,
			   bigint_element_t *result0 ) {
	unsigned int result_size = ( multiplicand_size + multiplier_size );
	const bigint_t ( multiplicand_size ) __attribute__ (( may_alias ))
		*multiplicand = ( ( const void * ) multiplicand0 );
	const bigint_t ( multiplier_size ) __attribute__ (( may_alias ))
		*multiplier = ( ( const void * ) multiplier0 );
	bigint_t ( result_size ) __attribute__ (( may_alias ))
		*result = ( ( void * ) result0 );
	bigint_element_t multiplicand_element;
	const bigint_element_t *multiplier_element;
	bigint_element_t *result_element;
	bigint_element_t carry_element;
	unsigned int i;
	unsigned int j;

	/* Zero required portion of result
	 *
	 * All elements beyond the length of the multiplier will be
	 * written before they are read, and so do not need to be
	 * zeroed in advance.
	 */
	memset ( result, 0, sizeof ( *multiplier ) );

	/* Multiply integers one element at a time, adding the low
	 * half of the double-element product directly into the
	 * result, and maintaining a running single-element carry.
	 *
	 * The running carry can never overflow beyond a single
	 * element.  At each step, the calculation we perform is:
	 *
	 *   carry:result[i+j] := ( ( multiplicand[i] * multiplier[j] )
	 *                          + result[i+j] + carry )
	 *
	 * The maximum value (for n-bit elements) is therefore:
	 *
	 *   (2^n - 1)*(2^n - 1) + (2^n - 1) + (2^n - 1) = 2^(2n) - 1
	 *
	 * This is precisely the maximum value for a 2n-bit integer,
	 * and so the carry out remains within the range of an n-bit
	 * integer, i.e. a single element.
	 */
	for ( i = 0 ; i < multiplicand_size ; i++ ) {
		multiplicand_element = multiplicand->element[i];
		multiplier_element = &multiplier->element[0];
		result_element = &result->element[i];
		carry_element = 0;
		for ( j = 0 ; j < multiplier_size ; j++ ) {
			bigint_multiply_one ( multiplicand_element,
					      *(multiplier_element++),
					      result_element++,
					      &carry_element );
		}
		*result_element = carry_element;
	}
}

/**
 * Perform modular multiplication of big integers
 *
 * @v multiplicand0	Element 0 of big integer to be multiplied
 * @v multiplier0	Element 0 of big integer to be multiplied
 * @v modulus0		Element 0 of big integer modulus
 * @v result0		Element 0 of big integer to hold result
 * @v size		Number of elements in base, modulus, and result
 * @v tmp		Temporary working space
 */
void bigint_mod_multiply_raw ( const bigint_element_t *multiplicand0,
			       const bigint_element_t *multiplier0,
			       const bigint_element_t *modulus0,
			       bigint_element_t *result0,
			       unsigned int size, void *tmp ) {
	const bigint_t ( size ) __attribute__ (( may_alias )) *multiplicand =
		( ( const void * ) multiplicand0 );
	const bigint_t ( size ) __attribute__ (( may_alias )) *multiplier =
		( ( const void * ) multiplier0 );
	const bigint_t ( size ) __attribute__ (( may_alias )) *modulus =
		( ( const void * ) modulus0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *result =
		( ( void * ) result0 );
	struct {
		bigint_t ( size * 2 ) result;
		bigint_t ( size * 2 ) modulus;
	} *temp = tmp;
	int shift;
	int i;

	/* Start profiling */
	profile_start ( &bigint_mod_multiply_profiler );

	/* Sanity check */
	assert ( sizeof ( *temp ) == bigint_mod_multiply_tmp_len ( modulus ) );

	/* Perform multiplication */
	profile_start ( &bigint_mod_multiply_multiply_profiler );
	bigint_multiply ( multiplicand, multiplier, &temp->result );
	profile_stop ( &bigint_mod_multiply_multiply_profiler );

	/* Rescale modulus to match result */
	profile_start ( &bigint_mod_multiply_rescale_profiler );
	bigint_grow ( modulus, &temp->modulus );
	shift = ( bigint_max_set_bit ( &temp->result ) -
		  bigint_max_set_bit ( &temp->modulus ) );
	for ( i = 0 ; i < shift ; i++ )
		bigint_shl ( &temp->modulus );
	profile_stop ( &bigint_mod_multiply_rescale_profiler );

	/* Subtract multiples of modulus */
	profile_start ( &bigint_mod_multiply_subtract_profiler );
	for ( i = 0 ; i <= shift ; i++ ) {
		if ( bigint_is_geq ( &temp->result, &temp->modulus ) )
			bigint_subtract ( &temp->modulus, &temp->result );
		bigint_shr ( &temp->modulus );
	}
	profile_stop ( &bigint_mod_multiply_subtract_profiler );

	/* Resize result */
	bigint_shrink ( &temp->result, result );

	/* Sanity check */
	assert ( bigint_is_geq ( modulus, result ) );

	/* Stop profiling */
	profile_stop ( &bigint_mod_multiply_profiler );
}

/**
 * Perform modular exponentiation of big integers
 *
 * @v base0		Element 0 of big integer base
 * @v modulus0		Element 0 of big integer modulus
 * @v exponent0		Element 0 of big integer exponent
 * @v result0		Element 0 of big integer to hold result
 * @v size		Number of elements in base, modulus, and result
 * @v exponent_size	Number of elements in exponent
 * @v tmp		Temporary working space
 */
void bigint_mod_exp_raw ( const bigint_element_t *base0,
			  const bigint_element_t *modulus0,
			  const bigint_element_t *exponent0,
			  bigint_element_t *result0,
			  unsigned int size, unsigned int exponent_size,
			  void *tmp ) {
	const bigint_t ( size ) __attribute__ (( may_alias )) *base =
		( ( const void * ) base0 );
	const bigint_t ( size ) __attribute__ (( may_alias )) *modulus =
		( ( const void * ) modulus0 );
	const bigint_t ( exponent_size ) __attribute__ (( may_alias ))
		*exponent = ( ( const void * ) exponent0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *result =
		( ( void * ) result0 );
	size_t mod_multiply_len = bigint_mod_multiply_tmp_len ( modulus );
	struct {
		bigint_t ( size ) base;
		bigint_t ( exponent_size ) exponent;
		uint8_t mod_multiply[mod_multiply_len];
	} *temp = tmp;
	static const uint8_t start[1] = { 0x01 };

	memcpy ( &temp->base, base, sizeof ( temp->base ) );
	memcpy ( &temp->exponent, exponent, sizeof ( temp->exponent ) );
	bigint_init ( result, start, sizeof ( start ) );

	while ( ! bigint_is_zero ( &temp->exponent ) ) {
		if ( bigint_bit_is_set ( &temp->exponent, 0 ) ) {
			bigint_mod_multiply ( result, &temp->base, modulus,
					      result, temp->mod_multiply );
		}
		bigint_shr ( &temp->exponent );
		bigint_mod_multiply ( &temp->base, &temp->base, modulus,
				      &temp->base, temp->mod_multiply );
	}
}
