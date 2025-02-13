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
#include <stdio.h>
#include <ipxe/bigint.h>

/** @file
 *
 * Big integer support
 */

/** Minimum number of least significant bytes included in transcription */
#define BIGINT_NTOA_LSB_MIN 16

/**
 * Transcribe big integer (for debugging)
 *
 * @v value0		Element 0 of big integer to be transcribed
 * @v size		Number of elements
 * @ret string		Big integer in string form (may be abbreviated)
 */
const char * bigint_ntoa_raw ( const bigint_element_t *value0,
			       unsigned int size ) {
	const bigint_t ( size ) __attribute__ (( may_alias ))
		*value = ( ( const void * ) value0 );
	bigint_element_t element;
	static char buf[256];
	unsigned int count;
	int threshold;
	uint8_t byte;
	char *tmp;
	int i;

	/* Calculate abbreviation threshold, if any */
	count = ( size * sizeof ( element ) );
	threshold = count;
	if ( count >= ( ( sizeof ( buf ) - 1 /* NUL */ ) / 2 /* "xx" */ ) ) {
		threshold -= ( ( sizeof ( buf ) - 3 /* "..." */
				 - ( BIGINT_NTOA_LSB_MIN * 2 /* "xx" */ )
				 - 1 /* NUL */ ) / 2 /* "xx" */ );
	}

	/* Transcribe bytes, abbreviating with "..." if necessary */
	for ( tmp = buf, i = ( count - 1 ) ; i >= 0 ; i-- ) {
		element = value->element[ i / sizeof ( element ) ];
		byte = ( element >> ( 8 * ( i % sizeof ( element ) ) ) );
		tmp += sprintf ( tmp, "%02x", byte );
		if ( i == threshold ) {
			tmp += sprintf ( tmp, "..." );
			i = BIGINT_NTOA_LSB_MIN;
		}
	}
	assert ( tmp < ( buf + sizeof ( buf ) ) );

	return buf;
}

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
 * Reduce big integer R^2 modulo N
 *
 * @v modulus0		Element 0 of big integer modulus
 * @v result0		Element 0 of big integer to hold result
 * @v size		Number of elements in modulus and result
 *
 * Reduce the value R^2 modulo N, where R=2^n and n is the number of
 * bits in the representation of the modulus N, including any leading
 * zero bits.
 */
void bigint_reduce_raw ( const bigint_element_t *modulus0,
			 bigint_element_t *result0, unsigned int size ) {
	const bigint_t ( size ) __attribute__ (( may_alias ))
		*modulus = ( ( const void * ) modulus0 );
	bigint_t ( size ) __attribute__ (( may_alias ))
		*result = ( ( void * ) result0 );
	const unsigned int width = ( 8 * sizeof ( bigint_element_t ) );
	unsigned int shift;
	int max;
	int sign;
	int msb;
	int carry;

	/* We have the constants:
	 *
	 *    N = modulus
	 *
	 *    n = number of bits in the modulus (including any leading zeros)
	 *
	 *    R = 2^n
	 *
	 * Let r be the extension of the n-bit result register by a
	 * separate two's complement sign bit, such that -R <= r < R,
	 * and define:
	 *
	 *    x = r * 2^k
	 *
	 * as the value being reduced modulo N, where k is a
	 * non-negative integer bit shift.
	 *
	 * We want to reduce the initial value R^2=2^(2n), which we
	 * may trivially represent using r=1 and k=2n.
	 *
	 * We then iterate over decrementing k, maintaining the loop
	 * invariant:
	 *
	 *    -N <= r < N
	 *
	 * On each iteration we must first double r, to compensate for
	 * having decremented k:
	 *
	 *    k' = k - 1
	 *
	 *    r' = 2r
	 *
	 *    x = r * 2^k = 2r * 2^(k-1) = r' * 2^k'
	 *
	 * Note that doubling the n-bit result register will create a
	 * value of n+1 bits: this extra bit needs to be handled
	 * separately during the calculation.
	 *
	 * We then subtract N (if r is currently non-negative) or add
	 * N (if r is currently negative) to restore the loop
	 * invariant:
	 *
	 *      0 <= r < N    =>   r" = 2r - N    =>   -N <= r" < N
	 *     -N <= r < 0    =>   r" = 2r + N    =>   -N <= r" < N
	 *
	 * Note that since N may use all n bits, the most significant
	 * bit of the n-bit result register is not a valid two's
	 * complement sign bit for r: the extra sign bit therefore
	 * also needs to be handled separately.
	 *
	 * Once we reach k=0, we have x=r and therefore:
	 *
	 *    -N <= x < N
	 *
	 * After this last loop iteration (with k=0), we may need to
	 * add a single multiple of N to ensure that x is positive,
	 * i.e. lies within the range 0 <= x < N.
	 *
	 * Since neither the modulus nor the value R^2 are secret, we
	 * may elide approximately half of the total number of
	 * iterations by constructing the initial representation of
	 * R^2 as r=2^m and k=2n-m (for some m such that 2^m < N).
	 */

	/* Initialise x=R^2 */
	memset ( result, 0, sizeof ( *result ) );
	max = ( bigint_max_set_bit ( modulus ) - 2 );
	if ( max < 0 ) {
		/* Degenerate case of N=0 or N=1: return a zero result */
		return;
	}
	bigint_set_bit ( result, max );
	shift = ( ( 2 * size * width ) - max );
	sign = 0;

	/* Iterate as described above */
	while ( shift-- ) {

		/* Calculate 2r, storing extra bit separately */
		msb = bigint_shl ( result );

		/* Add or subtract N according to current sign */
		if ( sign ) {
			carry = bigint_add ( modulus, result );
		} else {
			carry = bigint_subtract ( modulus, result );
		}

		/* Calculate new sign of result
		 *
		 * We know the result lies in the range -N <= r < N
		 * and so the tuple (old sign, msb, carry) cannot ever
		 * take the values (0, 1, 0) or (1, 0, 0).  We can
		 * therefore treat these as don't-care inputs, which
		 * allows us to simplify the boolean expression by
		 * ignoring the old sign completely.
		 */
		assert ( ( sign == msb ) || carry );
		sign = ( msb ^ carry );
	}

	/* Add N to make result positive if necessary */
	if ( sign )
		bigint_add ( modulus, result );

	/* Sanity check */
	assert ( ! bigint_is_geq ( result, modulus ) );
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
 * Perform relaxed Montgomery reduction (REDC) of a big integer
 *
 * @v modulus0		Element 0 of big integer odd modulus
 * @v value0		Element 0 of big integer to be reduced
 * @v result0		Element 0 of big integer to hold result
 * @v size		Number of elements in modulus and result
 * @ret carry		Carry out
 *
 * The value to be reduced will be made divisible by the size of the
 * modulus while retaining its residue class (i.e. multiples of the
 * modulus will be added until the low half of the value is zero).
 *
 * The result may be expressed as
 *
 *    tR = x + mN
 *
 * where x is the input value, N is the modulus, R=2^n (where n is the
 * number of bits in the representation of the modulus, including any
 * leading zero bits), and m is the number of multiples of the modulus
 * added to make the result tR divisible by R.
 *
 * The maximum addend is mN <= (R-1)*N (and such an m can be proven to
 * exist since N is limited to being odd and therefore coprime to R).
 *
 * Since the result of this addition is one bit larger than the input
 * value, a carry out bit is also returned.  The caller may be able to
 * prove that the carry out is always zero, in which case it may be
 * safely ignored.
 *
 * The upper half of the output value (i.e. t) will also be copied to
 * the result pointer.  It is permissible for the result pointer to
 * overlap the lower half of the input value.
 *
 * External knowledge of constraints on the modulus and the input
 * value may be used to prove constraints on the result.  The
 * constraint on the modulus may be generally expressed as
 *
 *    R > kN
 *
 * for some positive integer k.  The value k=1 is allowed, and simply
 * expresses that the modulus fits within the number of bits in its
 * own representation.
 *
 * For classic Montgomery reduction, we have k=1, i.e. R > N and a
 * separate constraint that the input value is in the range x < RN.
 * This gives the result constraint
 *
 *    tR < RN + (R-1)N
 *       < 2RN - N
 *       < 2RN
 *     t < 2N
 *
 * A single subtraction of the modulus may therefore be required to
 * bring it into the range t < N.
 *
 * When the input value is known to be a product of two integers A and
 * B, with A < aN and B < bN, we get the result constraint
 *
 *    tR < abN^2 + (R-1)N
 *       < (ab/k)RN + RN - N
 *       < (1 + ab/k)RN
 *     t < (1 + ab/k)N
 *
 * If we have k=a=b=1, i.e. R > N with A < N and B < N, then the
 * result is in the range t < 2N and may require a single subtraction
 * of the modulus to bring it into the range t < N so that it may be
 * used as an input on a subsequent iteration.
 *
 * If we have k=4 and a=b=2, i.e. R > 4N with A < 2N and B < 2N, then
 * the result is in the range t < 2N and may immediately be used as an
 * input on a subsequent iteration, without requiring a subtraction.
 *
 * Larger values of k may be used to allow for larger values of a and
 * b, which can be useful to elide intermediate reductions in a
 * calculation chain that involves additions and subtractions between
 * multiplications (as used in elliptic curve point addition, for
 * example).  As a general rule: each intermediate addition or
 * subtraction will require k to be doubled.
 *
 * When the input value is known to be a single integer A, with A < aN
 * (as used when converting out of Montgomery form), we get the result
 * constraint
 *
 *    tR < aN + (R-1)N
 *       < RN + (a-1)N
 *
 * If we have a=1, i.e. A < N, then the constraint becomes
 *
 *    tR < RN
 *     t < N
 *
 * and so the result is immediately in the range t < N with no
 * subtraction of the modulus required.
 *
 * For any larger value of a, the result value t=N becomes possible.
 * Additional external knowledge may potentially be used to prove that
 * t=N cannot occur.  For example: if the caller is performing modular
 * exponentiation with a prime modulus (or, more generally, a modulus
 * that is coprime to the base), then there is no way for a non-zero
 * base value to end up producing an exact multiple of the modulus.
 * If t=N cannot be disproved, then conversion out of Montgomery form
 * may require an additional subtraction of the modulus.
 */
int bigint_montgomery_relaxed_raw ( const bigint_element_t *modulus0,
				    bigint_element_t *value0,
				    bigint_element_t *result0,
				    unsigned int size ) {
	const bigint_t ( size ) __attribute__ (( may_alias ))
		*modulus = ( ( const void * ) modulus0 );
	union {
		bigint_t ( size * 2 ) full;
		struct {
			bigint_t ( size ) low;
			bigint_t ( size ) high;
		} __attribute__ (( packed ));
	} __attribute__ (( may_alias )) *value = ( ( void * ) value0 );
	bigint_t ( size ) __attribute__ (( may_alias ))
		*result = ( ( void * ) result0 );
	static bigint_t ( 1 ) cached;
	static bigint_t ( 1 ) negmodinv;
	bigint_element_t multiple;
	bigint_element_t carry;
	unsigned int i;
	unsigned int j;
	int overflow;

	/* Sanity checks */
	assert ( bigint_bit_is_set ( modulus, 0 ) );

	/* Calculate inverse (or use cached version) */
	if ( cached.element[0] != modulus->element[0] ) {
		bigint_mod_invert ( modulus, &negmodinv );
		negmodinv.element[0] = -negmodinv.element[0];
		cached.element[0] = modulus->element[0];
	}

	/* Perform multiprecision Montgomery reduction */
	for ( i = 0 ; i < size ; i++ ) {

		/* Determine scalar multiple for this round */
		multiple = ( value->low.element[i] * negmodinv.element[0] );

		/* Multiply value to make it divisible by 2^(width*i) */
		carry = 0;
		for ( j = 0 ; j < size ; j++ ) {
			bigint_multiply_one ( multiple, modulus->element[j],
					      &value->full.element[ i + j ],
					      &carry );
		}

		/* Since value is now divisible by 2^(width*i), we
		 * know that the current low element must have been
		 * zeroed.
		 */
		assert ( value->low.element[i] == 0 );

		/* Store the multiplication carry out in the result,
		 * avoiding the need to immediately propagate the
		 * carry through the remaining elements.
		 */
		result->element[i] = carry;
	}

	/* Add the accumulated carries */
	overflow = bigint_add ( result, &value->high );

	/* Copy to result buffer */
	bigint_copy ( &value->high, result );

	return overflow;
}

/**
 * Perform classic Montgomery reduction (REDC) of a big integer
 *
 * @v modulus0		Element 0 of big integer odd modulus
 * @v value0		Element 0 of big integer to be reduced
 * @v result0		Element 0 of big integer to hold result
 * @v size		Number of elements in modulus and result
 */
void bigint_montgomery_raw ( const bigint_element_t *modulus0,
			     bigint_element_t *value0,
			     bigint_element_t *result0,
			     unsigned int size ) {
	const bigint_t ( size ) __attribute__ (( may_alias ))
		*modulus = ( ( const void * ) modulus0 );
	union {
		bigint_t ( size * 2 ) full;
		struct {
			bigint_t ( size ) low;
			bigint_t ( size ) high;
		} __attribute__ (( packed ));
	} __attribute__ (( may_alias )) *value = ( ( void * ) value0 );
	bigint_t ( size ) __attribute__ (( may_alias ))
		*result = ( ( void * ) result0 );
	int overflow;
	int underflow;

	/* Sanity check */
	assert ( ! bigint_is_geq ( &value->high, modulus ) );

	/* Perform relaxed Montgomery reduction */
	overflow = bigint_montgomery_relaxed ( modulus, &value->full, result );

	/* Conditionally subtract the modulus once */
	underflow = bigint_subtract ( modulus, result );
	bigint_swap ( result, &value->high, ( underflow & ~overflow ) );

	/* Sanity check */
	assert ( ! bigint_is_geq ( result, modulus ) );
}

/**
 * Perform generalised exponentiation via a Montgomery ladder
 *
 * @v result0		Element 0 of result (initialised to identity element)
 * @v multiple0		Element 0 of multiple (initialised to generator)
 * @v size		Number of elements in result and multiple
 * @v exponent0		Element 0 of exponent
 * @v exponent_size	Number of elements in exponent
 * @v op		Montgomery ladder commutative operation
 * @v ctx		Operation context (if needed)
 * @v tmp		Temporary working space (if needed)
 *
 * The Montgomery ladder may be used to perform any operation that is
 * isomorphic to exponentiation, i.e. to compute the result
 *
 *    r = g^e = g * g * g * g * .... * g
 *
 * for an arbitrary commutative operation "*", generator "g" and
 * exponent "e".
 *
 * The result "r" is computed in constant time (assuming that the
 * underlying operation is constant time) in k steps, where k is the
 * number of bits in the big integer representation of the exponent.
 *
 * The result "r" must be initialised to the operation's identity
 * element, and the multiple must be initialised to the generator "g".
 * On exit, the multiple will contain
 *
 *    m = r * g = g^(e+1)
 *
 * Note that the terminology used here refers to exponentiation
 * defined as repeated multiplication, but that the ladder may equally
 * well be used to perform any isomorphic operation (such as
 * multiplication defined as repeated addition).
 */
void bigint_ladder_raw ( bigint_element_t *result0,
			 bigint_element_t *multiple0, unsigned int size,
			 const bigint_element_t *exponent0,
			 unsigned int exponent_size, bigint_ladder_op_t *op,
			 const void *ctx, void *tmp ) {
	bigint_t ( size ) __attribute__ (( may_alias ))
		*result = ( ( void * ) result0 );
	bigint_t ( size ) __attribute__ (( may_alias ))
		*multiple = ( ( void * ) multiple0 );
	const bigint_t ( exponent_size ) __attribute__ (( may_alias ))
		*exponent = ( ( const void * ) exponent0 );
	const unsigned int width = ( 8 * sizeof ( bigint_element_t ) );
	unsigned int bit = ( exponent_size * width );
	int toggle = 0;
	int previous;

	/* We have two physical storage locations: Rres (the "result"
	 * register) and Rmul (the "multiple" register).
	 *
	 * On entry we have:
	 *
	 *   Rres = g^0 = 1  (the identity element)
	 *   Rmul = g        (the generator)
	 *
	 * For calculation purposes, define two virtual registers R[0]
	 * and R[1], mapped to the underlying physical storage
	 * locations via a boolean toggle state "t":
	 *
	 *   R[t]  -> Rres
	 *   R[~t] -> Rmul
	 *
	 * Define the initial toggle state to be t=0, so that we have:
	 *
	 *   R[0] = g^0 = 1  (the identity element)
	 *   R[1] = g        (the generator)
	 *
	 * The Montgomery ladder then iterates over each bit "b" of
	 * the exponent "e" (from MSB down to LSB), computing:
	 *
	 *   R[1]  := R[0] * R[1],    R[0]  := R[0] * R[0]    (if b=0)
	 *   R[0]  := R[1] * R[0],    R[1]  := R[1] * R[1]    (if b=1)
	 *
	 * i.e.
	 *
	 *   R[~b] := R[b] * R[~b],   R[b]  := R[b] * R[b]
	 *
	 * To avoid data-dependent memory access patterns, we
	 * implement this as:
	 *
	 *   Rmul := Rres * Rmul
	 *   Rres := Rres * Rres
	 *
	 * and update the toggle state "t" (via constant-time
	 * conditional swaps) as needed to ensure that R[b] always
	 * maps to Rres and R[~b] always maps to Rmul.
	 */
	while ( bit-- ) {

		/* Update toggle state t := b
		 *
		 * If the new toggle state is not equal to the old
		 * toggle state, then we must swap R[0] and R[1] (in
		 * constant time).
		 */
		previous = toggle;
		toggle = bigint_bit_is_set ( exponent, bit );
		bigint_swap ( result, multiple, ( toggle ^ previous ) );

		/* Calculate Rmul = R[~b] := R[b] * R[~b] = Rres * Rmul */
		op ( result->element, multiple->element, size, ctx, tmp );

		/* Calculate Rres = R[b]  := R[b] * R[b]  = Rres * Rres */
		op ( result->element, result->element, size, ctx, tmp );
	}

	/* Reset toggle state to zero */
	bigint_swap ( result, multiple, toggle );
}

/**
 * Perform modular multiplication as part of a Montgomery ladder
 *
 * @v operand		Element 0 of first input operand (may overlap result)
 * @v result		Element 0 of second input operand and result
 * @v size		Number of elements in operands and result
 * @v ctx		Operation context (odd modulus, or NULL)
 * @v tmp		Temporary working space
 */
void bigint_mod_exp_ladder ( const bigint_element_t *multiplier0,
			     bigint_element_t *result0, unsigned int size,
			     const void *ctx, void *tmp ) {
	const bigint_t ( size ) __attribute__ (( may_alias ))
		*multiplier = ( ( const void * ) multiplier0 );
	bigint_t ( size ) __attribute__ (( may_alias ))
		*result = ( ( void * ) result0 );
	const bigint_t ( size ) __attribute__ (( may_alias ))
		*modulus = ( ( const void * ) ctx );
	bigint_t ( size * 2 ) __attribute__ (( may_alias ))
		*product = ( ( void * ) tmp );

	/* Multiply and reduce */
	bigint_multiply ( result, multiplier, product );
	if ( modulus ) {
		bigint_montgomery ( modulus, product, result );
	} else {
		bigint_shrink ( product, result );
	}
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
		struct {
			bigint_t ( size ) modulus;
			bigint_t ( size ) stash;
		};
		union {
			bigint_t ( 2 * size ) full;
			bigint_t ( size ) low;
		} product;
	} *temp = tmp;
	const uint8_t one[1] = { 1 };
	bigint_element_t submask;
	unsigned int subsize;
	unsigned int scale;
	unsigned int i;

	/* Sanity check */
	assert ( sizeof ( *temp ) == bigint_mod_exp_tmp_len ( modulus ) );

	/* Handle degenerate case of zero modulus */
	if ( ! bigint_max_set_bit ( modulus ) ) {
		memset ( result, 0, sizeof ( *result ) );
		return;
	}

	/* Factor modulus as (N * 2^scale) where N is odd */
	bigint_copy ( modulus, &temp->modulus );
	for ( scale = 0 ; ( ! bigint_bit_is_set ( &temp->modulus, 0 ) ) ;
	      scale++ ) {
		bigint_shr ( &temp->modulus );
	}
	subsize = ( ( scale + width - 1 ) / width );
	submask = ( ( 1UL << ( scale % width ) ) - 1 );
	if ( ! submask )
		submask = ~submask;

	/* Calculate (R^2 mod N) */
	bigint_reduce ( &temp->modulus, &temp->stash );

	/* Initialise result = Montgomery(1, R^2 mod N) */
	bigint_grow ( &temp->stash, &temp->product.full );
	bigint_montgomery ( &temp->modulus, &temp->product.full, result );

	/* Convert base into Montgomery form */
	bigint_multiply ( base, &temp->stash, &temp->product.full );
	bigint_montgomery ( &temp->modulus, &temp->product.full,
			    &temp->stash );

	/* Calculate x1 = base^exponent modulo N */
	bigint_ladder ( result, &temp->stash, exponent, bigint_mod_exp_ladder,
			&temp->modulus, &temp->product );

	/* Convert back out of Montgomery form */
	bigint_grow ( result, &temp->product.full );
	bigint_montgomery_relaxed ( &temp->modulus, &temp->product.full,
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
		bigint_copy ( subbase, submodulus );
		bigint_ladder ( substash, submodulus, exponent,
				bigint_mod_exp_ladder, NULL, subproduct );

		/* Reconstruct N */
		bigint_copy ( modulus, &temp->modulus );
		for ( i = 0 ; i < scale ; i++ )
			bigint_shr ( &temp->modulus );

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
