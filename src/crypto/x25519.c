/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
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
 * X25519 key exchange
 *
 * This implementation is inspired by and partially based upon the
 * paper "Implementing Curve25519/X25519: A Tutorial on Elliptic Curve
 * Cryptography" by Martin Kleppmann, available for download from
 * https://www.cl.cam.ac.uk/teaching/2122/Crypto/curve25519.pdf
 *
 * The underlying modular addition, subtraction, and multiplication
 * operations are completely redesigned for substantially improved
 * efficiency compared to the TweetNaCl implementation studied in that
 * paper.
 *
 *				       TweetNaCl	    iPXE
 *				       ---------	    ----
 *
 * Storage size of each big integer	     128	      40
 * (in bytes)
 *
 * Stack usage for key exchange		    1144	     360
 * (in bytes, large objects only)
 *
 * Cost of big integer addition		      16	       5
 * (in number of 64-bit additions)
 *
 * Cost of big integer multiplication	     273	      31
 * (in number of 64-bit multiplications)
 *
 * The implementation is constant-time (provided that the underlying
 * big integer operations are also constant-time).
 */

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ipxe/init.h>
#include <ipxe/crypto.h>
#include <ipxe/x25519.h>

/** X25519 reduction constant
 *
 * The X25519 field prime is p=2^255-19.  This gives us:
 *
 *               p = 2^255 - 19
 *           2^255 = p + 19
 *           2^255 = 19          (mod p)
 *       k * 2^255 = k * 19      (mod p)
 *
 * We can therefore reduce a value modulo p by taking the high-order
 * bits of the value from bit 255 and above, multiplying by 19, and
 * adding this to the low-order 255 bits of the value.
 *
 * This would be cumbersome to do in practice since it would require
 * partitioning the value at a 255-bit boundary (and hence would
 * require some shifting and masking operations).  However, we can
 * note that:
 *
 *       k * 2^255 = k * 19      (mod p)
 *   k * 2 * 2^255 = k * 2 * 19  (mod p)
 *       k * 2^256 = k * 38      (mod p)
 *
 * We can therefore simplify the reduction to taking the high order
 * bits of the value from bit 256 and above, multiplying by 38, and
 * adding this to the low-order 256 bits of the value.
 *
 * Since 256 will inevitably be a multiple of the big integer element
 * size (typically 32 or 64 bits), this avoids the need to perform any
 * shifting or masking operations.
 */
#define X25519_REDUCE_256 38

/** X25519 multiplication step 1 result
 *
 * Step 1 of X25519 multiplication is to compute the product of two
 * X25519 unsigned 258-bit integers.
 *
 * Both multiplication inputs are limited to 258 bits, and so the
 * product will have at most 516 bits.
 */
union x25519_multiply_step1 {
	/** Raw product
	 *
	 * Big integer multiplication produces a result with a number
	 * of elements equal to the sum of the number of elements in
	 * each input.
	 */
	bigint_t ( X25519_SIZE + X25519_SIZE ) product;
	/** Partition into low-order and high-order bits
	 *
	 * Reduction modulo p requires separating the low-order 256
	 * bits from the remaining high-order bits.
	 *
	 * Since the value will never exceed 516 bits (see above),
	 * there will be at most 260 high-order bits.
	 */
	struct {
		/** Low-order 256 bits */
		bigint_t ( bigint_required_size ( ( 256 /* bits */ + 7 ) / 8 ) )
			low_256bit;
		/** High-order 260 bits */
		bigint_t ( bigint_required_size ( ( 260 /* bits */ + 7 ) / 8 ) )
			high_260bit;
	} __attribute__ (( packed )) parts;
};

/** X25519 multiplication step 2 result
 *
 * Step 2 of X25519 multiplication is to multiply the high-order 260
 * bits from step 1 with the 6-bit reduction constant 38, and to add
 * this to the low-order 256 bits from step 1.
 *
 * The multiplication inputs are limited to 260 and 6 bits
 * respectively, and so the product will have at most 266 bits.  After
 * adding the low-order 256 bits from step 1, the result will have at
 * most 267 bits.
 */
union x25519_multiply_step2 {
	/** Raw product
	 *
	 * Big integer multiplication produces a result with a number
	 * of elements equal to the sum of the number of elements in
	 * each input.
	 */
	bigint_t ( bigint_required_size ( ( 260 /* bits */ + 7 ) / 8 ) +
		   bigint_required_size ( ( 6 /* bits */ + 7 ) / 8 ) ) product;
	/** Big integer value
	 *
	 * The value will never exceed 267 bits (see above), and so
	 * may be consumed as a normal X25519 big integer.
	 */
	x25519_t value;
	/** Partition into low-order and high-order bits
	 *
	 * Reduction modulo p requires separating the low-order 256
	 * bits from the remaining high-order bits.
	 *
	 * Since the value will never exceed 267 bits (see above),
	 * there will be at most 11 high-order bits.
	 */
	struct {
		/** Low-order 256 bits */
		bigint_t ( bigint_required_size ( ( 256 /* bits */ + 7 ) / 8 ) )
			low_256bit;
		/** High-order 11 bits */
		bigint_t ( bigint_required_size ( ( 11 /* bits */ + 7 ) / 8 ) )
			high_11bit;
	} __attribute__ (( packed )) parts;
};

/** X25519 multiplication step 3 result
 *
 * Step 3 of X25519 multiplication is to multiply the high-order 11
 * bits from step 2 with the 6-bit reduction constant 38, and to add
 * this to the low-order 256 bits from step 2.
 *
 * The multiplication inputs are limited to 11 and 6 bits
 * respectively, and so the product will have at most 17 bits.  After
 * adding the low-order 256 bits from step 2, the result will have at
 * most 257 bits.
 */
union x25519_multiply_step3 {
	/** Raw product
	 *
	 * Big integer multiplication produces a result with a number
	 * of elements equal to the sum of the number of elements in
	 * each input.
	 */
	bigint_t ( bigint_required_size ( ( 11 /* bits */ + 7 ) / 8 ) +
		   bigint_required_size ( ( 6 /* bits */ + 7 ) / 8 ) ) product;
	/** Big integer value
	 *
	 * The value will never exceed 267 bits (see above), and so
	 * may be consumed as a normal X25519 big integer.
	 */
	x25519_t value;
};

/** X25519 multiplication temporary working space
 *
 * We overlap the buffers used by each step of the multiplication
 * calculation to reduce the total stack space required:
 *
 * |--------------------------------------------------------|
 * | <- pad -> | <------------ step 1 result -------------> |
 * |           | <- low 256 bits -> | <-- high 260 bits --> |
 * | <------- step 2 result ------> | <-- step 3 result --> |
 * |--------------------------------------------------------|
 */
union x25519_multiply_workspace {
	/** Step 1 result */
	struct {
		/** Padding to avoid collision between steps 1 and 2
		 *
		 * The step 2 multiplication consumes the high 260
		 * bits of step 1, and so the step 2 multiplication
		 * result must not overlap this portion of the step 1
		 * result.
		 */
		uint8_t pad[ sizeof ( union x25519_multiply_step2 ) -
			     offsetof ( union x25519_multiply_step1,
					parts.high_260bit ) ];
		/** Step 1 result */
		union x25519_multiply_step1 step1;
	} __attribute__ (( packed ));
	/** Steps 2 and 3 results */
	struct {
		/** Step 2 result */
		union x25519_multiply_step2 step2;
		/** Step 3 result */
		union x25519_multiply_step3 step3;
	} __attribute__ (( packed ));
};

/** An X25519 elliptic curve point in projective coordinates
 *
 * A point (x,y) on the Montgomery curve used in X25519 is represented
 * using projective coordinates (X/Z,Y/Z) so that intermediate
 * calculations may be performed on both numerator and denominator
 * separately, with the division step performed only once at the end
 * of the calculation.
 *
 * The group operation calculation is performed using a Montgomery
 * ladder as:
 *
 *   X[2i]   = ( X[i]^2 - Z[i]^2 )^2
 *   X[2i+1] = ( X[i] * X[i+1] - Z[i] * Z[i+1] )^2
 *   Z[2i]   = 4 * X[i] * Z[i] * ( X[i]^2 + A * X[i] * Z[i] + Z[i]^2 )
 *   Z[2i+1] = X[0] * ( X[i] * Z[i+1] - X[i+1] * Z[i] ) ^ 2
 *
 * It is therefore not necessary to store (or use) the value of Y.
 */
struct x25519_projective {
	/** X coordinate */
	union x25519_quad257 X;
	/** Z coordinate */
	union x25519_quad257 Z;
};

/** An X25519 Montgomery ladder step */
struct x25519_step {
	/** X[n]/Z[n] */
	struct x25519_projective x_n;
	/** X[n+1]/Z[n+1] */
	struct x25519_projective x_n1;
};

/** Constant p=2^255-19 (the finite field prime) */
static const uint8_t x25519_p_raw[] = {
	0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xed
};

/** Constant p=2^255-19 (the finite field prime) */
static x25519_t x25519_p;

/** Constant 2p=2^256-38 */
static x25519_t x25519_2p;

/** Constant 4p=2^257-76 */
static x25519_t x25519_4p;

/** Reduction constant (used during multiplication) */
static const uint8_t x25519_reduce_256_raw[] = { X25519_REDUCE_256 };

/** Reduction constant (used during multiplication) */
static bigint_t ( bigint_required_size ( sizeof ( x25519_reduce_256_raw ) ) )
	x25519_reduce_256;

/** Constant 121665 (used in the Montgomery ladder) */
static const uint8_t x25519_121665_raw[] = { 0x01, 0xdb, 0x41 };

/** Constant 121665 (used in the Montgomery ladder) */
static union x25519_oct258 x25519_121665;

/** Constant g=9 (the group generator) */
static struct x25519_value x25519_generator = {
	.raw = { 9, }
};

/**
 * Initialise constants
 *
 */
static void x25519_init_constants ( void ) {

	/* Construct constant p */
	bigint_init ( &x25519_p, x25519_p_raw, sizeof ( x25519_p_raw ) );

	/* Construct constant 2p */
	bigint_copy ( &x25519_p, &x25519_2p );
	bigint_add ( &x25519_p, &x25519_2p );

	/* Construct constant 4p */
	bigint_copy ( &x25519_2p, &x25519_4p );
	bigint_add ( &x25519_2p, &x25519_4p );

	/* Construct reduction constant */
	bigint_init ( &x25519_reduce_256, x25519_reduce_256_raw,
		      sizeof ( x25519_reduce_256_raw ) );

	/* Construct constant 121665 */
	bigint_init ( &x25519_121665.value, x25519_121665_raw,
		      sizeof ( x25519_121665_raw ) );
}

/** Initialisation function */
struct init_fn x25519_init_fn __init_fn ( INIT_NORMAL ) = {
	.initialise = x25519_init_constants,
};

/**
 * Add big integers modulo field prime
 *
 * @v augend		Big integer to add
 * @v addend		Big integer to add
 * @v result		Big integer to hold result (may overlap augend)
 */
static inline __attribute__ (( always_inline )) void
x25519_add ( const union x25519_quad257 *augend,
	     const union x25519_quad257 *addend,
	     union x25519_oct258 *result ) {
	int copy;

	/* Copy augend if necessary */
	copy = ( result != &augend->oct258 );
	build_assert ( __builtin_constant_p ( copy ) );
	if ( copy ) {
		build_assert ( result != &addend->oct258 );
		bigint_copy ( &augend->oct258.value, &result->value );
	}

	/* Perform addition
	 *
	 * Both inputs are in the range [0,4p-1] and the resulting
	 * sum is therefore in the range [0,8p-2].
	 *
	 * This range lies within the range [0,8p-1] and the result is
	 * therefore a valid X25519 unsigned 258-bit integer, as
	 * required.
	 */
	bigint_add ( &addend->value, &result->value );
}

/**
 * Subtract big integers modulo field prime
 *
 * @v minuend		Big integer from which to subtract
 * @v subtrahend	Big integer to subtract
 * @v result		Big integer to hold result (may overlap minuend)
 */
static inline __attribute__ (( always_inline )) void
x25519_subtract ( const union x25519_quad257 *minuend,
		  const union x25519_quad257 *subtrahend,
		  union x25519_oct258 *result ) {
	int copy;

	/* Copy minuend if necessary */
	copy = ( result != &minuend->oct258 );
	build_assert ( __builtin_constant_p ( copy ) );
	if ( copy ) {
		build_assert ( result != &subtrahend->oct258 );
		bigint_copy ( &minuend->oct258.value, &result->value );
	}

	/* Perform subtraction
	 *
	 * Both inputs are in the range [0,4p-1] and the resulting
	 * difference is therefore in the range [1-4p,4p-1].
	 *
	 * This range lies partially outside the range [0,8p-1] and
	 * the result is therefore not yet a valid X25519 unsigned
	 * 258-bit integer.
	 */
	bigint_subtract ( &subtrahend->value, &result->value );

	/* Add constant multiple of field prime p
	 *
	 * Add the constant 4p to the result.  This brings the result
	 * within the range [1,8p-1] (without changing the value
	 * modulo p).
	 *
	 * This range lies within the range [0,8p-1] and the result is
	 * therefore now a valid X25519 unsigned 258-bit integer, as
	 * required.
	 */
	bigint_add ( &x25519_4p, &result->value );
}

/**
 * Multiply big integers modulo field prime
 *
 * @v multiplicand	Big integer to be multiplied
 * @v multiplier	Big integer to be multiplied
 * @v result		Big integer to hold result (may overlap either input)
 */
void x25519_multiply ( const union x25519_oct258 *multiplicand,
		       const union x25519_oct258 *multiplier,
		       union x25519_quad257 *result ) {
	union x25519_multiply_workspace tmp;
	union x25519_multiply_step1 *step1 = &tmp.step1;
	union x25519_multiply_step2 *step2 = &tmp.step2;
	union x25519_multiply_step3 *step3 = &tmp.step3;

	/* Step 1: perform raw multiplication
	 *
	 *   step1 = multiplicand * multiplier
	 *
	 * Both inputs are 258-bit numbers and the step 1 result is
	 * therefore 258+258=516 bits.
	 */
	static_assert ( sizeof ( step1->product ) >= sizeof ( step1->parts ) );
	bigint_multiply ( &multiplicand->value, &multiplier->value,
			  &step1->product );

	/* Step 2: reduce high-order 516-256=260 bits of step 1 result
	 *
	 * Use the identity 2^256=38 (mod p) to reduce the high-order
	 * bits of the step 1 result.  We split the 516-bit result
	 * from step 1 into its low-order 256 bits and high-order 260
	 * bits:
	 *
	 *   step1 = step1(low 256 bits) + step1(high 260 bits) * 2^256
	 *
	 * and then perform the calculation:
	 *
	 *   step2 = step1                                              (mod p)
	 *         = step1(low 256 bits) + step1(high 260 bits) * 2^256 (mod p)
	 *         = step1(low 256 bits) + step1(high 260 bits) * 38    (mod p)
	 *
	 * There are 6 bits in the constant value 38.  The step 2
	 * multiplication product will therefore have 260+6=266 bits,
	 * and the step 2 result (after the addition) will therefore
	 * have 267 bits.
	 */
	static_assert ( sizeof ( step2->product ) >= sizeof ( step2->value ) );
	static_assert ( sizeof ( step2->product ) >= sizeof ( step2->parts ) );
	bigint_grow ( &step1->parts.low_256bit, &result->value );
	bigint_multiply ( &step1->parts.high_260bit, &x25519_reduce_256,
			  &step2->product );
	bigint_add ( &result->value, &step2->value );

	/* Step 3: reduce high-order 267-256=11 bits of step 2 result
	 *
	 * Use the identity 2^256=38 (mod p) again to reduce the
	 * high-order bits of the step 2 result.  As before, we split
	 * the 267-bit result from step 2 into its low-order 256 bits
	 * and high-order 11 bits:
	 *
	 *   step2 = step2(low 256 bits) + step2(high 11 bits) * 2^256
	 *
	 * and then perform the calculation:
	 *
	 *   step3 = step2                                             (mod p)
	 *         = step2(low 256 bits) + step2(high 11 bits) * 2^256 (mod p)
	 *         = step2(low 256 bits) + step2(high 11 bits) * 38    (mod p)
	 *
	 * There are 6 bits in the constant value 38.  The step 3
	 * multiplication product will therefore have 11+6=19 bits,
	 * and the step 3 result (after the addition) will therefore
	 * have 257 bits.
	 *
	 * A loose upper bound for the step 3 result (after the
	 * addition) is given by:
	 *
	 *   step3 < ( 2^256 - 1 ) + ( 2^19 - 1 )
	 *         < ( 2^257 - 2^256 - 1 ) + ( 2^19 - 1 )
	 *         < ( 2^257 - 76 ) - 2^256 + 2^19 + 74
	 *         < 4 * ( 2^255 - 19 ) - 2^256 + 2^19 + 74
	 *         < 4p - 2^256 + 2^19 + 74
	 *
	 * and so the step 3 result is strictly less than 4p, and
	 * therefore lies within the range [0,4p-1].
	 */
	memset ( &step3->value, 0, sizeof ( step3->value ) );
	bigint_grow ( &step2->parts.low_256bit, &result->value );
	bigint_multiply ( &step2->parts.high_11bit, &x25519_reduce_256,
			  &step3->product );
	bigint_add ( &step3->value, &result->value );

	/* Step 1 calculates the product of the input operands, and
	 * each subsequent step reduces the number of bits in the
	 * result while preserving this value (modulo p).  The final
	 * result is therefore equal to the product of the input
	 * operands (modulo p), as required.
	 *
	 * The step 3 result lies within the range [0,4p-1] and the
	 * final result is therefore a valid X25519 unsigned 257-bit
	 * integer, as required.
	 */
}

/**
 * Compute multiplicative inverse
 *
 * @v invertend		Big integer to be inverted
 * @v result		Big integer to hold result (may not overlap input)
 */
void x25519_invert ( const union x25519_oct258 *invertend,
		     union x25519_quad257 *result ) {
	int i;

	/* Sanity check */
	assert ( invertend != &result->oct258 );

	/* Calculate inverse as x^(-1)=x^(p-2) where p is the field prime
	 *
	 * The field prime is p=2^255-19 and so:
	 *
	 *   p - 2 = 2^255 - 21
	 *         = (2^255 - 1) - 2^4 - 2^2
	 *
	 * i.e. p-2 is a 254-bit number in which all bits are set
	 * apart from bit 2 and bit 4.
	 *
	 * We use the square-and-multiply method to compute x^(p-2).
	 */
	bigint_copy ( &invertend->value, &result->value );
	for ( i = 253 ; i >= 0 ; i-- ) {

		/* Square running total */
		x25519_multiply ( &result->oct258, &result->oct258, result );

		/* For each set bit in the exponent, multiply by invertend */
		if ( ( i != 2 ) && ( i != 4 ) ) {
			x25519_multiply ( invertend, &result->oct258, result );
		}
	}
}

/**
 * Reduce big integer via conditional subtraction
 *
 * @v subtrahend	Big integer to subtract
 * @v value		Big integer to be subtracted from, if possible
 */
static void x25519_reduce_by ( const x25519_t *subtrahend, x25519_t *value ) {
	x25519_t tmp;
	int underflow;

	/* Conditionally subtract subtrahend
	 *
	 * Subtract the subtrahend, discarding the result (in constant
	 * time) if the subtraction underflows.
	 */
	bigint_copy ( value, &tmp );
	underflow = bigint_subtract ( subtrahend, value );
	bigint_swap ( value, &tmp, underflow );
}

/**
 * Reduce big integer to canonical range
 *
 * @v value		Big integer to be reduced
 */
void x25519_reduce ( union x25519_quad257 *value ) {

	/* Conditionally subtract 2p
	 *
	 * Subtract twice the field prime, discarding the result (in
	 * constant time) if the subtraction underflows.
	 *
	 * The input value is in the range [0,4p-1].  After this
	 * conditional subtraction, the value is in the range
	 * [0,2p-1].
	 */
	x25519_reduce_by ( &x25519_2p, &value->value );

	/* Conditionally subtract p
	 *
	 * Subtract the field prime, discarding the result (in
	 * constant time) if the subtraction underflows.
	 *
	 * The value is already in the range [0,2p-1].  After this
	 * conditional subtraction, the value is in the range [0,p-1]
	 * and is therefore the canonical representation.
	 */
	x25519_reduce_by ( &x25519_p, &value->value );
}

/**
 * Compute next step of the Montgomery ladder
 *
 * @v base		Base point
 * @v bit		Bit value
 * @v step		Ladder step
 */
static void x25519_step ( const union x25519_quad257 *base, int bit,
			  struct x25519_step *step ) {
	union x25519_quad257 *a = &step->x_n.X;
	union x25519_quad257 *b = &step->x_n1.X;
	union x25519_quad257 *c = &step->x_n.Z;
	union x25519_quad257 *d = &step->x_n1.Z;
	union x25519_oct258 e;
	union x25519_quad257 f;
	union x25519_oct258 *v1_e;
	union x25519_oct258 *v2_a;
	union x25519_oct258 *v3_c;
	union x25519_oct258 *v4_b;
	union x25519_quad257 *v5_d;
	union x25519_quad257 *v6_f;
	union x25519_quad257 *v7_a;
	union x25519_quad257 *v8_c;
	union x25519_oct258 *v9_e;
	union x25519_oct258 *v10_a;
	union x25519_quad257 *v11_b;
	union x25519_oct258 *v12_c;
	union x25519_quad257 *v13_a;
	union x25519_oct258 *v14_a;
	union x25519_quad257 *v15_c;
	union x25519_quad257 *v16_a;
	union x25519_quad257 *v17_d;
	union x25519_quad257 *v18_b;

	/* See the referenced paper "Implementing Curve25519/X25519: A
	 * Tutorial on Elliptic Curve Cryptography" for the reasoning
	 * behind this calculation.
	 */

	/* Reuse storage locations for intermediate results where possible */
	v1_e = &e;
	v2_a = container_of ( &a->value, union x25519_oct258, value );
	v3_c = container_of ( &c->value, union x25519_oct258, value );
	v4_b = container_of ( &b->value, union x25519_oct258, value );
	v5_d = d;
	v6_f = &f;
	v7_a = a;
	v8_c = c;
	v9_e = &e;
	v10_a = container_of ( &a->value, union x25519_oct258, value );
	v11_b = b;
	v12_c = container_of ( &c->value, union x25519_oct258, value );
	v13_a = a;
	v14_a = container_of ( &a->value, union x25519_oct258, value );
	v15_c = c;
	v16_a = a;
	v17_d = d;
	v18_b = b;

	/* Select inputs */
	bigint_swap ( &a->value, &b->value, bit );
	bigint_swap ( &c->value, &d->value, bit );

	/* v1 = a + c */
	x25519_add ( a, c, v1_e );

	/* v2 = a - c */
	x25519_subtract ( a, c, v2_a );

	/* v3 = b + d */
	x25519_add ( b, d, v3_c );

	/* v4 = b - d */
	x25519_subtract ( b, d, v4_b );

	/* v5 = v1^2 = (a + c)^2 = a^2 + 2ac + c^2 */
	x25519_multiply ( v1_e, v1_e, v5_d );

	/* v6 = v2^2 = (a - c)^2 = a^2 - 2ac + c^2 */
	x25519_multiply ( v2_a, v2_a, v6_f );

	/* v7 = v3 * v2 = (b + d) * (a - c) = ab - bc + ad - cd */
	x25519_multiply ( v3_c, v2_a, v7_a );

	/* v8 = v4 * v1 = (b - d) * (a + c) = ab + bc - ad - cd */
	x25519_multiply ( v4_b, v1_e, v8_c );

	/* v9 = v7 + v8 = 2 * (ab - cd) */
	x25519_add ( v7_a, v8_c, v9_e );

	/* v10 = v7 - v8 = 2 * (ad - bc) */
	x25519_subtract ( v7_a, v8_c, v10_a );

	/* v11 = v10^2 = 4 * (ad - bc)^2 */
	x25519_multiply ( v10_a, v10_a, v11_b );

	/* v12 = v5 - v6 = (a + c)^2 - (a - c)^2 = 4ac */
	x25519_subtract ( v5_d, v6_f, v12_c );

	/* v13 = v12 * 121665 = 486660ac = (A-2) * ac */
	x25519_multiply ( v12_c, &x25519_121665, v13_a );

	/* v14 = v13 + v5 = (A-2) * ac + a^2 + 2ac + c^2 = a^2 + A * ac + c^2 */
	x25519_add ( v13_a, v5_d, v14_a );

	/* v15 = v12 * v14 = 4ac * (a^2 + A * ac + c^2) */
	x25519_multiply ( v12_c, v14_a, v15_c );

	/* v16 = v5 * v6 = (a + c)^2 * (a - c)^2 = (a^2 - c^2)^2 */
	x25519_multiply ( &v5_d->oct258, &v6_f->oct258, v16_a );

	/* v17 = v11 * base = 4 * base * (ad - bc)^2 */
	x25519_multiply ( &v11_b->oct258, &base->oct258, v17_d );

	/* v18 = v9^2 = 4 * (ab - cd)^2 */
	x25519_multiply ( v9_e, v9_e, v18_b );

	/* Select outputs */
	bigint_swap ( &a->value, &b->value, bit );
	bigint_swap ( &c->value, &d->value, bit );
}

/**
 * Multiply X25519 elliptic curve point
 *
 * @v base		Base point
 * @v scalar		Scalar multiple
 * @v result		Point to hold result (may overlap base point)
 */
static void x25519_ladder ( const union x25519_quad257 *base,
			    struct x25519_value *scalar,
			    union x25519_quad257 *result ) {
	static const uint8_t zero[] = { 0 };
	static const uint8_t one[] = { 1 };
	struct x25519_step step;
	union x25519_quad257 *tmp;
	int bit;
	int i;

	/* Initialise ladder */
	bigint_init ( &step.x_n.X.value, one, sizeof ( one ) );
	bigint_init ( &step.x_n.Z.value, zero, sizeof ( zero ) );
	bigint_copy ( &base->value, &step.x_n1.X.value );
	bigint_init ( &step.x_n1.Z.value, one, sizeof ( one ) );

	/* Use ladder */
	for ( i = 254 ; i >= 0 ; i-- ) {
		bit = ( ( scalar->raw[ i / 8 ] >> ( i % 8 ) ) & 1 );
		x25519_step ( base, bit, &step );
	}

	/* Convert back to affine coordinate */
	tmp = &step.x_n1.X;
	x25519_invert ( &step.x_n.Z.oct258, tmp );
	x25519_multiply ( &step.x_n.X.oct258, &tmp->oct258, result );
	x25519_reduce ( result );
}

/**
 * Reverse X25519 value endianness
 *
 * @v value		Value to reverse
 */
static void x25519_reverse ( struct x25519_value *value ) {
	uint8_t *low = value->raw;
	uint8_t *high = &value->raw[ sizeof ( value->raw ) - 1 ];
	uint8_t tmp;

	/* Reverse bytes */
	do {
		tmp = *low;
		*low = *high;
		*high = tmp;
	} while ( ++low < --high );
}

/**
 * Calculate X25519 key
 *
 * @v base		Base point
 * @v scalar		Scalar multiple
 * @v result		Point to hold result (may overlap base point)
 * @ret rc		Return status code
 */
int x25519_key ( const struct x25519_value *base,
		 const struct x25519_value *scalar,
		 struct x25519_value *result ) {
	struct x25519_value *tmp = result;
	union x25519_quad257 point;

	/* Reverse base point and clear high bit as required by RFC7748 */
	memcpy ( tmp, base, sizeof ( *tmp ) );
	x25519_reverse ( tmp );
	tmp->raw[0] &= 0x7f;
	bigint_init ( &point.value, tmp->raw, sizeof ( tmp->raw ) );

	/* Clamp scalar as required by RFC7748 */
	memcpy ( tmp, scalar, sizeof ( *tmp ) );
	tmp->raw[0] &= 0xf8;
	tmp->raw[31] |= 0x40;

	/* Multiply elliptic curve point */
	x25519_ladder ( &point, tmp, &point );

	/* Reverse result */
	bigint_done ( &point.value, result->raw, sizeof ( result->raw ) );
	x25519_reverse ( result );

	/* Fail if result was all zeros (as required by RFC8422) */
	return ( bigint_is_zero ( &point.value ) ? -EPERM : 0 );
}

/**
 * Multiply scalar by curve point
 *
 * @v base		Base point (or NULL to use generator)
 * @v scalar		Scalar multiple
 * @v result		Result point to fill in
 * @ret rc		Return status code
 */
static int x25519_curve_multiply ( const void *base, const void *scalar,
				   void *result ) {

	/* Use base point if applicable */
	if ( ! base )
		base = &x25519_generator;

	return x25519_key ( base, scalar, result );
}

/** X25519 elliptic curve */
struct elliptic_curve x25519_curve = {
	.name = "x25519",
	.pointsize = sizeof ( struct x25519_value ),
	.keysize = sizeof ( struct x25519_value ),
	.multiply = x25519_curve_multiply,
};
