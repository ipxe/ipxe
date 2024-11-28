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
 * @v modulus0		Element 0 of big integer modulus
 * @v value0		Element 0 of big integer to be reduced
 * @v size		Number of elements in modulus and value
 */
void bigint_reduce_raw ( bigint_element_t *modulus0, bigint_element_t *value0,
			 unsigned int size ) {
	bigint_t ( size ) __attribute__ (( may_alias ))
		*modulus = ( ( void * ) modulus0 );
	bigint_t ( size ) __attribute__ (( may_alias ))
		*value = ( ( void * ) value0 );
	const unsigned int width = ( 8 * sizeof ( bigint_element_t ) );
	bigint_element_t *element;
	unsigned int modulus_max;
	unsigned int value_max;
	unsigned int subshift;
	int offset;
	int shift;
	int msb;
	int i;

	/* Start profiling */
	profile_start ( &bigint_mod_profiler );

	/* Normalise the modulus
	 *
	 * Scale the modulus by shifting left such that both modulus
	 * "m" and value "x" have the same most significant set bit.
	 * (If this is not possible, then the value is already less
	 * than the modulus, and we may therefore skip reduction
	 * completely.)
	 */
	value_max = bigint_max_set_bit ( value );
	modulus_max = bigint_max_set_bit ( modulus );
	shift = ( value_max - modulus_max );
	if ( shift < 0 )
		goto skip;
	subshift = ( shift & ( width - 1 ) );
	offset = ( shift / width );
	element = modulus->element;
	for ( i = ( ( value_max - 1 ) / width ) ; ; i-- ) {
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

	/* Reduce the value "x" by iteratively adding or subtracting
	 * the scaled modulus "m".
	 *
	 * On each loop iteration, we maintain the invariant:
	 *
	 *    -2m <= x < 2m
	 *
	 * If x is positive, we obtain the new value x' by
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
			bigint_add ( modulus, value );
		} else {
			bigint_subtract ( modulus, value );
		}
		msb = bigint_msb_is_set ( value );
		if ( shift > 0 )
			bigint_shr ( modulus );
	}

 skip:
	/* Sanity check */
	assert ( ! bigint_is_geq ( value, modulus ) );

	/* Stop profiling */
	profile_stop ( &bigint_mod_profiler );
}

/**
 * Compute inverse of odd big integer modulo any power of two
 *
 * @v invertend0	Element 0 of odd big integer to be inverted
 * @v inverse0		Element 0 of big integer to hold result
 * @v size		Number of elements in invertend and result
 */
void bigint_mod_invert_raw ( const bigint_element_t *invertend0,
			     bigint_element_t *inverse0, unsigned int size ) {
	const bigint_t ( size ) __attribute__ (( may_alias ))
		*invertend = ( ( const void * ) invertend0 );
	bigint_t ( size ) __attribute__ (( may_alias ))
		*inverse = ( ( void * ) inverse0 );
	bigint_element_t accum;
	bigint_element_t bit;
	unsigned int i;

	/* Sanity check */
	assert ( bigint_bit_is_set ( invertend, 0 ) );

	/* Initialise output */
	memset ( inverse, 0xff, sizeof ( *inverse ) );

	/* Compute inverse modulo 2^(width)
	 *
	 * This method is a lightly modified version of the pseudocode
	 * presented in "A New Algorithm for Inversion mod p^k (Koç,
	 * 2017)".
	 *
	 * Each inner loop iteration calculates one bit of the
	 * inverse.  The residue value is the two's complement
	 * negation of the value "b" as used by Koç, to allow for
	 * division by two using a logical right shift (since we have
	 * no arithmetic right shift operation for big integers).
	 *
	 * The residue is stored in the as-yet uncalculated portion of
	 * the inverse.  The size of the residue therefore decreases
	 * by one element for each outer loop iteration.  Trivial
	 * inspection of the algorithm shows that any higher bits
	 * could not contribute to the eventual output value, and so
	 * we may safely reuse storage this way.
	 *
	 * Due to the suffix property of inverses mod 2^k, the result
	 * represents the least significant bits of the inverse modulo
	 * an arbitrarily large 2^k.
	 */
	for ( i = size ; i > 0 ; i-- ) {
		const bigint_t ( i ) __attribute__ (( may_alias ))
			*addend = ( ( const void * ) invertend );
		bigint_t ( i ) __attribute__ (( may_alias ))
			*residue = ( ( void * ) inverse );

		/* Calculate one element's worth of inverse bits */
		for ( accum = 0, bit = 1 ; bit ; bit <<= 1 ) {
			if ( bigint_bit_is_set ( residue, 0 ) ) {
				accum |= bit;
				bigint_add ( addend, residue );
			}
			bigint_shr ( residue );
		}

		/* Store in the element no longer required to hold residue */
		inverse->element[ i - 1 ] = accum;
	}

	/* Correct order of inverse elements */
	for ( i = 0 ; i < ( size / 2 ) ; i++ ) {
		accum = inverse->element[i];
		inverse->element[i] = inverse->element[ size - 1 - i ];
		inverse->element[ size - 1 - i ] = accum;
	}
}

/**
 * Perform Montgomery reduction (REDC) of a big integer product
 *
 * @v modulus0		Element 0 of big integer modulus
 * @v modinv0		Element 0 of the inverse of the modulus modulo 2^k
 * @v mont0		Element 0 of big integer Montgomery product
 * @v result0		Element 0 of big integer to hold result
 * @v size		Number of elements in modulus and result
 *
 * Note that only the least significant element of the inverse modulo
 * 2^k is required, and that the Montgomery product will be
 * overwritten.
 */
void bigint_montgomery_raw ( const bigint_element_t *modulus0,
			     const bigint_element_t *modinv0,
			     bigint_element_t *mont0,
			     bigint_element_t *result0, unsigned int size ) {
	const bigint_t ( size ) __attribute__ (( may_alias ))
		*modulus = ( ( const void * ) modulus0 );
	const bigint_t ( 1 ) __attribute__ (( may_alias ))
		*modinv = ( ( const void * ) modinv0 );
	union {
		bigint_t ( size * 2 ) full;
		struct {
			bigint_t ( size ) low;
			bigint_t ( size ) high;
		} __attribute__ (( packed ));
	} __attribute__ (( may_alias )) *mont = ( ( void * ) mont0 );
	bigint_t ( size ) __attribute__ (( may_alias ))
		*result = ( ( void * ) result0 );
	bigint_element_t negmodinv = -modinv->element[0];
	bigint_element_t multiple;
	bigint_element_t carry;
	unsigned int i;
	unsigned int j;
	int overflow;
	int underflow;

	/* Sanity checks */
	assert ( bigint_bit_is_set ( modulus, 0 ) );

	/* Perform multiprecision Montgomery reduction */
	for ( i = 0 ; i < size ; i++ ) {

		/* Determine scalar multiple for this round */
		multiple = ( mont->low.element[i] * negmodinv );

		/* Multiply value to make it divisible by 2^(width*i) */
		carry = 0;
		for ( j = 0 ; j < size ; j++ ) {
			bigint_multiply_one ( multiple, modulus->element[j],
					      &mont->full.element[ i + j ],
					      &carry );
		}

		/* Since value is now divisible by 2^(width*i), we
		 * know that the current low element must have been
		 * zeroed.  We can store the multiplication carry out
		 * in this element, avoiding the need to immediately
		 * propagate the carry through the remaining elements.
		 */
		assert ( mont->low.element[i] == 0 );
		mont->low.element[i] = carry;
	}

	/* Add the accumulated carries */
	overflow = bigint_add ( &mont->low, &mont->high );

	/* Conditionally subtract the modulus once */
	memcpy ( result, &mont->high, sizeof ( *result ) );
	underflow = bigint_subtract ( modulus, result );
	bigint_swap ( result, &mont->high, ( underflow & ~overflow ) );

	/* Sanity check */
	assert ( ! bigint_is_geq ( result, modulus ) );
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
	const unsigned int width = ( 8 * sizeof ( bigint_element_t ) );
	struct {
		union {
			bigint_t ( 2 * size ) padded_modulus;
			struct {
				bigint_t ( size ) modulus;
				bigint_t ( size ) stash;
			};
		};
		union {
			bigint_t ( 2 * size ) full;
			bigint_t ( size ) low;
		} product;
	} *temp = tmp;
	const uint8_t one[1] = { 1 };
	bigint_t ( 1 ) modinv;
	bigint_element_t submask;
	unsigned int subsize;
	unsigned int scale;
	unsigned int max;
	unsigned int bit;

	/* Sanity check */
	assert ( sizeof ( *temp ) == bigint_mod_exp_tmp_len ( modulus ) );

	/* Handle degenerate case of zero modulus */
	if ( ! bigint_max_set_bit ( modulus ) ) {
		memset ( result, 0, sizeof ( *result ) );
		return;
	}

	/* Factor modulus as (N * 2^scale) where N is odd */
	bigint_grow ( modulus, &temp->padded_modulus );
	for ( scale = 0 ; ( ! bigint_bit_is_set ( &temp->modulus, 0 ) ) ;
	      scale++ ) {
		bigint_shr ( &temp->modulus );
	}
	subsize = ( ( scale + width - 1 ) / width );
	submask = ( ( 1UL << ( scale % width ) ) - 1 );
	if ( ! submask )
		submask = ~submask;

	/* Calculate inverse of (scaled) modulus N modulo element size */
	bigint_mod_invert ( &temp->modulus, &modinv );

	/* Calculate (R^2 mod N) via direct reduction of (R^2 - N) */
	memset ( &temp->product.full, 0, sizeof ( temp->product.full ) );
	bigint_subtract ( &temp->padded_modulus, &temp->product.full );
	bigint_reduce ( &temp->padded_modulus, &temp->product.full );
	bigint_copy ( &temp->product.low, &temp->stash );

	/* Initialise result = Montgomery(1, R^2 mod N) */
	bigint_montgomery ( &temp->modulus, &modinv,
			    &temp->product.full, result );

	/* Convert base into Montgomery form */
	bigint_multiply ( base, &temp->stash, &temp->product.full );
	bigint_montgomery ( &temp->modulus, &modinv, &temp->product.full,
			    &temp->stash );

	/* Calculate x1 = base^exponent modulo N */
	max = bigint_max_set_bit ( exponent );
	for ( bit = 1 ; bit <= max ; bit++ ) {

		/* Square (and reduce) */
		bigint_multiply ( result, result, &temp->product.full );
		bigint_montgomery ( &temp->modulus, &modinv,
				    &temp->product.full, result );

		/* Multiply (and reduce) */
		bigint_multiply ( &temp->stash, result, &temp->product.full );
		bigint_montgomery ( &temp->modulus, &modinv,
				    &temp->product.full, &temp->product.low );

		/* Conditionally swap the multiplied result */
		bigint_swap ( result, &temp->product.low,
			      bigint_bit_is_set ( exponent, ( max - bit ) ) );
	}

	/* Convert back out of Montgomery form */
	bigint_grow ( result, &temp->product.full );
	bigint_montgomery ( &temp->modulus, &modinv, &temp->product.full,
			    result );

	/* Handle even moduli via Garner's algorithm */
	if ( subsize ) {
		const bigint_t ( subsize ) __attribute__ (( may_alias ))
			*subbase = ( ( const void * ) base );
		bigint_t ( subsize ) __attribute__ (( may_alias ))
			*submodulus = ( ( void * ) &temp->modulus );
		bigint_t ( subsize ) __attribute__ (( may_alias ))
			*substash = ( ( void * ) &temp->stash );
		bigint_t ( subsize ) __attribute__ (( may_alias ))
			*subresult = ( ( void * ) result );
		union {
			bigint_t ( 2 * subsize ) full;
			bigint_t ( subsize ) low;
		} __attribute__ (( may_alias ))
			*subproduct = ( ( void * ) &temp->product.full );

		/* Calculate x2 = base^exponent modulo 2^k */
		bigint_init ( substash, one, sizeof ( one ) );
		for ( bit = 1 ; bit <= max ; bit++ ) {

			/* Square (and reduce) */
			bigint_multiply ( substash, substash,
					  &subproduct->full );
			bigint_copy ( &subproduct->low, substash );

			/* Multiply (and reduce) */
			bigint_multiply ( subbase, substash,
					  &subproduct->full );

			/* Conditionally swap the multiplied result */
			bigint_swap ( substash, &subproduct->low,
				      bigint_bit_is_set ( exponent,
							  ( max - bit ) ) );
		}

		/* Calculate N^-1 modulo 2^k */
		bigint_mod_invert ( submodulus, &subproduct->low );
		bigint_copy ( &subproduct->low, submodulus );

		/* Calculate y = (x2 - x1) * N^-1 modulo 2^k */
		bigint_subtract ( subresult, substash );
		bigint_multiply ( substash, submodulus, &subproduct->full );
		subproduct->low.element[ subsize - 1 ] &= submask;
		bigint_grow ( &subproduct->low, &temp->stash );

		/* Reconstruct N */
		bigint_mod_invert ( submodulus, &subproduct->low );
		bigint_copy ( &subproduct->low, submodulus );

		/* Calculate x = x1 + N * y */
		bigint_multiply ( &temp->modulus, &temp->stash,
				  &temp->product.full );
		bigint_add ( &temp->product.low, result );
	}
}
