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
 * @v carry0		Element 0 of big integer to hold temporary carry
 */
void bigint_multiply_raw ( const bigint_element_t *multiplicand0,
			   unsigned int multiplicand_size,
			   const bigint_element_t *multiplier0,
			   unsigned int multiplier_size,
			   bigint_element_t *result0,
			   bigint_element_t *carry0 ) {
	unsigned int result_size = ( multiplicand_size + multiplier_size );
	const bigint_t ( multiplicand_size ) __attribute__ (( may_alias ))
		*multiplicand = ( ( const void * ) multiplicand0 );
	const bigint_t ( multiplier_size ) __attribute__ (( may_alias ))
		*multiplier = ( ( const void * ) multiplier0 );
	bigint_t ( result_size ) __attribute__ (( may_alias ))
		*result = ( ( void * ) result0 );
	bigint_t ( result_size ) __attribute__ (( may_alias ))
		*carry = ( ( void * ) carry0 );
	bigint_element_t multiplicand_element;
	const bigint_element_t *multiplier_element;
	bigint_element_t *result_elements;
	bigint_element_t *carry_element;
	unsigned int i;
	unsigned int j;

	/* Zero result and temporary carry space */
	memset ( result, 0, sizeof ( *result ) );
	memset ( carry, 0, sizeof ( *carry ) );

	/* Multiply integers one element at a time, adding the double
	 * element directly into the result and accumulating any
	 * overall carry out from this double-element addition into
	 * the temporary carry space.
	 *
	 * We could propagate the carry immediately instead of using a
	 * temporary carry space.  However, this would cause the
	 * multiplication to run in non-constant time, which is
	 * undesirable.
	 *
	 * The carry elements can never overflow, provided that the
	 * element size is large enough to accommodate any plausible
	 * big integer.  The total number of potential carries (across
	 * all elements) is the sum of the number of elements in the
	 * multiplicand and multiplier.  With a 16-bit element size,
	 * this therefore allows for up to a 1Mbit multiplication
	 * result (e.g. a 512kbit integer multiplied by another
	 * 512kbit integer), which is around 100x higher than could be
	 * needed in practice.  With a more realistic 32-bit element
	 * size, the limit becomes a totally implausible 128Gbit
	 * multiplication result.
	 */
	for ( i = 0 ; i < multiplicand_size ; i++ ) {
		multiplicand_element = multiplicand->element[i];
		multiplier_element = &multiplier->element[0];
		result_elements = &result->element[i];
		carry_element = &carry->element[i];
		for ( j = 0 ; j < multiplier_size ; j++ ) {
			bigint_multiply_one ( multiplicand_element,
					      *(multiplier_element++),
					      result_elements++,
					      carry_element++ );
		}
	}

	/* Add the temporary carry into the result.  The least
	 * significant element of the carry represents the carry out
	 * from multiplying the least significant elements of the
	 * multiplicand and multiplier, and therefore must be added to
	 * the third-least significant element of the result (i.e. the
	 * carry needs to be shifted left by two elements before being
	 * adding to the result).
	 *
	 * The most significant two elements of the carry are
	 * guaranteed to be zero, since:
	 *
	 *     a < 2^{n}, b < 2^{m} => ab < 2^{n+m}
	 *
	 * and the overall result of the multiplication (including
	 * adding in the shifted carries) is therefore guaranteed not
	 * to overflow beyond the end of the result.
	 *
	 * We could avoid this shifting by writing the carry directly
	 * into the "correct" element during the element-by-element
	 * multiplication stage above.  However, this would add
	 * complexity to the loop since we would have to arrange for
	 * the (provably zero) most significant two carry out results
	 * to be discarded, in order to avoid writing beyond the end
	 * of the temporary carry space.
	 *
	 * Performing the logical shift is essentially free, since we
	 * simply adjust the element pointers.
	 *
	 * To avoid requiring additional checks in each architecture's
	 * implementation of bigint_add_raw(), we explicitly avoid
	 * calling bigint_add_raw() with a size of zero.
	 */
	if ( result_size > 2 ) {
		bigint_add_raw ( &carry->element[0], &result->element[2],
				 ( result_size - 2 ) );
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
		union {
			bigint_t ( size * 2 ) modulus;
			bigint_t ( size * 2 ) carry;
		};
	} *temp = tmp;
	int rotation;
	int i;

	/* Start profiling */
	profile_start ( &bigint_mod_multiply_profiler );

	/* Sanity check */
	assert ( sizeof ( *temp ) == bigint_mod_multiply_tmp_len ( modulus ) );

	/* Perform multiplication */
	profile_start ( &bigint_mod_multiply_multiply_profiler );
	bigint_multiply ( multiplicand, multiplier, &temp->result,
			  &temp->carry );
	profile_stop ( &bigint_mod_multiply_multiply_profiler );

	/* Rescale modulus to match result */
	profile_start ( &bigint_mod_multiply_rescale_profiler );
	bigint_grow ( modulus, &temp->modulus );
	rotation = ( bigint_max_set_bit ( &temp->result ) -
		     bigint_max_set_bit ( &temp->modulus ) );
	for ( i = 0 ; i < rotation ; i++ )
		bigint_rol ( &temp->modulus );
	profile_stop ( &bigint_mod_multiply_rescale_profiler );

	/* Subtract multiples of modulus */
	profile_start ( &bigint_mod_multiply_subtract_profiler );
	for ( i = 0 ; i <= rotation ; i++ ) {
		if ( bigint_is_geq ( &temp->result, &temp->modulus ) )
			bigint_subtract ( &temp->modulus, &temp->result );
		bigint_ror ( &temp->modulus );
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
		bigint_ror ( &temp->exponent );
		bigint_mod_multiply ( &temp->base, &temp->base, modulus,
				      &temp->base, temp->mod_multiply );
	}
}
