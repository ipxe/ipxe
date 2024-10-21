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

/** Modular direct reduction profiler */
static struct profiler bigint_mod_profiler __profiler =
	{ .name = "bigint_mod" };

/** Modular multiplication overall profiler */
static struct profiler bigint_mod_multiply_profiler __profiler =
	{ .name = "bigint_mod_multiply" };

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
 * Reduce big integer
 *
 * @v minuend0		Element 0 of big integer to be reduced
 * @v minuend_size	Number of elements in minuend
 * @v modulus0		Element 0 of big integer modulus
 * @v modulus_size	Number of elements in modulus and result
 * @v result0		Element 0 of big integer to hold result
 * @v tmp		Temporary working space
 */
void bigint_reduce_raw ( const bigint_element_t *minuend0,
			 unsigned int minuend_size,
			 const bigint_element_t *modulus0,
			 unsigned int modulus_size,
			 bigint_element_t *result0, void *tmp ) {
	const bigint_t ( minuend_size ) __attribute__ (( may_alias ))
		*minuend = ( ( const void * ) minuend0 );
	const bigint_t ( modulus_size ) __attribute__ (( may_alias ))
		*modulus = ( ( const void * ) modulus0 );
	bigint_t ( modulus_size ) __attribute__ (( may_alias ))
		*result = ( ( void * ) result0 );
	struct {
		bigint_t ( minuend_size ) minuend;
		bigint_t ( minuend_size ) modulus;
	} *temp = tmp;
	const unsigned int width = ( 8 * sizeof ( bigint_element_t ) );
	const bigint_element_t msb_mask = ( 1UL << ( width - 1 ) );
	bigint_element_t *element;
	unsigned int minuend_max;
	unsigned int modulus_max;
	unsigned int subshift;
	bigint_element_t msb;
	int offset;
	int shift;
	int i;

	/* Start profiling */
	profile_start ( &bigint_mod_profiler );

	/* Sanity check */
	assert ( minuend_size >= modulus_size );
	assert ( sizeof ( *temp ) == bigint_reduce_tmp_len ( minuend ) );

	/* Copy minuend and modulus to temporary working space */
	bigint_shrink ( minuend, &temp->minuend );
	bigint_grow ( modulus, &temp->modulus );

	/* Normalise the modulus
	 *
	 * Scale the modulus by shifting left such that both modulus
	 * "m" and minuend "x" have the same most significant set bit.
	 * (If this is not possible, then the minuend is already less
	 * than the modulus, and we may therefore skip reduction
	 * completely.)
	 */
	minuend_max = bigint_max_set_bit ( minuend );
	modulus_max = bigint_max_set_bit ( modulus );
	shift = ( minuend_max - modulus_max );
	if ( shift < 0 )
		goto skip;
	subshift = ( shift & ( width - 1 ) );
	offset = ( shift / width );
	element = temp->modulus.element;
	for ( i = ( ( minuend_max - 1 ) / width ) ; ; i-- ) {
		element[i] = ( element[ i - offset ] << subshift );
		if ( i <= offset )
			break;
		if ( subshift ) {
			element[i] |= ( element[ i - offset - 1 ]
					>> ( width - subshift ) );
		}
	}
	for ( i-- ; i >= 0 ; i-- )
		element[i] = 0;

	/* Reduce the minuend "x" by iteratively adding or subtracting
	 * the scaled modulus "m".
	 *
	 * On each loop iteration, we maintain the invariant:
	 *
	 *    -2m <= x < 2m
	 *
	 * If x is positive, we obtain the new minuend x' by
	 * subtracting m, otherwise we add m:
	 *
	 *      0 <= x < 2m   =>   x' := x - m   =>   -m <= x' < m
	 *    -2m <= x < 0    =>   x' := x + m   =>   -m <= x' < m
	 *
	 * and then halve the modulus (by shifting right):
	 *
	 *      m' = m/2
	 *
	 * We therefore end up with:
	 *
	 *     -m <= x' < m   =>   -2m' <= x' < 2m'
	 *
	 * i.e. we have preseved the invariant while reducing the
	 * bounds on x' by one power of two.
	 *
	 * The issue remains of how to determine on each iteration
	 * whether or not x is currently positive, given that both
	 * input values are unsigned big integers that may use all
	 * available bits (including the MSB).
	 *
	 * On the first loop iteration, we may simply assume that x is
	 * positive, since it is unmodified from the input value and
	 * so is positive by definition (even if the MSB is set).  We
	 * therefore unconditionally perform a subtraction on the
	 * first loop iteration.
	 *
	 * Let k be the MSB after normalisation.  We then have:
	 *
	 *    2^k <= m < 2^(k+1)
	 *    2^k <= x < 2^(k+1)
	 *
	 * On the first loop iteration, we therefore have:
	 *
	 *     x' = (x - m)
	 *        < 2^(k+1) - 2^k
	 *        < 2^k
	 *
	 * Any positive value of x' therefore has its MSB set to zero,
	 * and so we may validly treat the MSB of x' as a sign bit at
	 * the end of the first loop iteration.
	 *
	 * On all subsequent loop iterations, the starting value m is
	 * guaranteed to have its MSB set to zero (since it has
	 * already been shifted right at least once).  Since we know
	 * from above that we preserve the loop invariant:
	 *
	 *     -m <= x' < m
	 *
	 * we immediately know that any positive value of x' also has
	 * its MSB set to zero, and so we may validly treat the MSB of
	 * x' as a sign bit at the end of all subsequent loop
	 * iterations.
	 *
	 * After the last loop iteration (when m' has been shifted
	 * back down to the original value of the modulus), we may
	 * need to add a single multiple of m' to ensure that x' is
	 * positive, i.e. lies within the range 0 <= x' < m'.  To
	 * allow for reusing the (inlined) expansion of
	 * bigint_subtract(), we achieve this via a potential
	 * additional loop iteration that performs the addition and is
	 * then guaranteed to terminate (since the result will be
	 * positive).
	 */
	for ( msb = 0 ; ( msb || ( shift >= 0 ) ) ; shift-- ) {
		if ( msb ) {
			bigint_add ( &temp->modulus, &temp->minuend );
		} else {
			bigint_subtract ( &temp->modulus, &temp->minuend );
		}
		msb = ( temp->minuend.element[ minuend_size - 1 ] & msb_mask );
		if ( shift > 0 )
			bigint_shr ( &temp->modulus );
	}

 skip:
	/* Sanity check */
	assert ( ! bigint_is_geq ( &temp->minuend, &temp->modulus ) );

	/* Copy result */
	bigint_shrink ( &temp->minuend, result );

	/* Stop profiling */
	profile_stop ( &bigint_mod_profiler );
}

/**
 * Compute inverse of odd big integer modulo any power of two
 *
 * @v invertend0	Element 0 of odd big integer to be inverted
 * @v inverse0		Element 0 of big integer to hold result
 * @v size		Number of elements in invertend and result
 * @v tmp		Temporary working space
 */
void bigint_mod_invert_raw ( const bigint_element_t *invertend0,
			     bigint_element_t *inverse0,
			     unsigned int size, void *tmp ) {
	const bigint_t ( size ) __attribute__ (( may_alias ))
		*invertend = ( ( const void * ) invertend0 );
	bigint_t ( size ) __attribute__ (( may_alias ))
		*inverse = ( ( void * ) inverse0 );
	struct {
		bigint_t ( size ) residue;
	} *temp = tmp;
	const unsigned int width = ( 8 * sizeof ( bigint_element_t ) );
	unsigned int i;

	/* Sanity check */
	assert ( invertend->element[0] & 1 );

	/* Initialise temporary working space and output value */
	memset ( &temp->residue, 0xff, sizeof ( temp->residue ) );
	memset ( inverse, 0, sizeof ( *inverse ) );

	/* Compute inverse modulo 2^(width)
	 *
	 * This method is a lightly modified version of the pseudocode
	 * presented in "A New Algorithm for Inversion mod p^k (Koç,
	 * 2017)".
	 *
	 * Each loop iteration calculates one bit of the inverse.  The
	 * residue value is the two's complement negation of the value
	 * "b" as used by Koç, to allow for division by two using a
	 * logical right shift (since we have no arithmetic right
	 * shift operation for big integers).
	 *
	 * Due to the suffix property of inverses mod 2^k, the result
	 * represents the least significant bits of the inverse modulo
	 * an arbitrarily large 2^k.
	 */
	for ( i = 0 ; i < ( 8 * sizeof ( *inverse ) ) ; i++ ) {
		if ( temp->residue.element[0] & 1 ) {
			inverse->element[ i / width ] |=
				( 1UL << ( i % width ) );
			bigint_add ( invertend, &temp->residue );
		}
		bigint_shr ( &temp->residue );
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

	/* Start profiling */
	profile_start ( &bigint_mod_multiply_profiler );

	/* Sanity check */
	assert ( sizeof ( *temp ) == bigint_mod_multiply_tmp_len ( modulus ) );

	/* Perform multiplication */
	bigint_multiply ( multiplicand, multiplier, &temp->result );

	/* Reduce result */
	bigint_reduce ( &temp->result, modulus, result, temp );

	/* Sanity check */
	assert ( ! bigint_is_geq ( result, modulus ) );

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
