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
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ipxe/bigint.h>

/** @file
 *
 * Big integer support
 */

/**
 * Calculate (val << shift)
 *
 * @v val               Value to shift
 * @v size		Number of elements in val
 * @v shift             Number of bits to shift to
 */
static inline
void bigint_shift_left_raw( bigint_element_t *val, unsigned int size, unsigned int shift )
{
	register unsigned int i, j, x;
	const int sz = sizeof(bigint_element_t) * 8;

	if ( 0 == shift )
		return;

	i = shift / sz;
	j = shift % sz;

	if ( ! i ) {
		for ( x = 1; x <= size-1; x++ ) {
			val[size-x] = ( val[size-x] << j ) | ( val[size-1-x] >> (sz-j) );
		}
		val[0] = val[0] << j;
	} else if ( ! j ) {
		// memmove here
		for ( x = 1; x <= size-i; x++ ) {
			val[size-x] = val[size-i-x];
		}
		// memset here
		for ( ; x <= size; x++ ) {
			val[size-x] = 0;
		}
	} else {
		for ( x = 1; x < size-i; x++ ) {
			val[size-x] = ( val[size-i-x] << j ) | ( val[size-i-1-x] >> (sz-j) );
		}
		val[size-x] = val[size-i-x] << j;
		for ( x++; x <= size; x++ ) {
			val[size-x] = 0;
		}
	}
}

#ifdef BIGINT_MONTGOMERY
/**
 * Calculate (val >> shift)
 *
 * @v val               Value to shift
 * @v size		Number of elements in val
 * @v shift             Number of bits to shift to
 */
static inline
void bigint_shift_right_raw( bigint_element_t *val, unsigned int size, unsigned int shift )
{
	register unsigned int i, j, x;
	const int sz = sizeof( bigint_element_t ) * 8;

	if ( 0 == shift )
		return;

	i = shift / sz;
	j = shift % sz;

	if ( ! i ) {
		for ( x = 0; x < size-1; x++ ) {
			val[x] = ( val[x] >> j ) | ( val[x+1] << (sz-j) );
		}
		val[size-1] = val[size-1] >> j;
	} else if ( ! j ) {
		for ( x = 0; x < size-i; x++ ) {
			val[x] = val[x+i];
		}
		for ( ; x < size; x++ ) {
			val[x] = 0;
		}
	} else {
		for (x = 0; x < size-i-1; x++) {
			val[x] = ( val[x+i] >> j ) | ( val[x+i+1] << (sz-j) );
		}
		val[x] = val[x+i] >> j;
		for ( x++; x < size; x++ ) {
			val[x] = 0;
		}
	}
}

/**
 * result = 1 << power 
 *
 * @v result		return value
 * @v power		Position of bit to be set
 * @v size		Number of elements in result
 */
void bigint_set_power2 ( bigint_element_t *result0,
			unsigned int power,
			unsigned int size ) {
	bigint_t ( size ) __attribute__ (( may_alias )) *result =
		( ( void * ) result0 );
	const unsigned int bits_per_element = sizeof( bigint_element_t ) * 8;

	assert(power < size * bits_per_element);

	memset(result, 0, sizeof(*result));
	result->element[power/bits_per_element] = 1 << ( power % bits_per_element );
}

/**
 * result |= 1 << power 
 *
 * @v result		return value
 * @v bit		Position of bit to be set
 * @v size		Number of elements in result
 */
static void bigint_set_bit ( bigint_element_t *result0,
				unsigned int bit,
				unsigned int size ) {
	bigint_t ( size ) __attribute__ (( may_alias )) *result =
		( ( void * ) result0 );
	const unsigned int bits_per_element = sizeof( bigint_element_t ) * 8;

	assert(bit < size * bits_per_element);

	result->element[bit/bits_per_element] |= 1 << (bit % bits_per_element);
}

/**
 * quotient = dividend / divisor 
 *
 * @v dividend0		Element 0 of dividend
 * @v divisor0		Element 0 of divisor
 * @v quotient0		Element 0 of big integer to hold quotient
 * @v size		Number of elements in dividend, divisor and quotient
 * @v tmp		Temporary working space
 */
static
void bigint_div_raw ( const bigint_element_t *dividend0,
		const bigint_element_t *divisor0,
		bigint_element_t *quotient0,
		unsigned int size,
		void *tmp ) {
	const bigint_t ( size ) __attribute__ (( may_alias )) *dividend =
		( ( const void * ) dividend0 );
	const bigint_t ( size ) __attribute__ (( may_alias )) *divisor =
		( ( const void * ) divisor0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *quotient =
		( ( void * ) quotient0 );

	unsigned int i, rotation;
	static const uint8_t zero[1] = { 0x00 };
	struct {
		bigint_t ( size ) dividend;
		bigint_t ( size ) divisor;
	} *temp = tmp;

	bigint_init( quotient, zero, sizeof(zero) );
	/* return 0 if ( divisor > dividend ) */
	if ( bigint_is_geq( divisor, dividend ) ) {
		return;
	}
	/* copy numbers */
	bigint_shrink( dividend, &temp->dividend );
	bigint_shrink( divisor, &temp->divisor );
	rotation = ( bigint_max_set_bit ( &temp->dividend ) -
		bigint_max_set_bit ( &temp->divisor ) );

	if ( 0 < rotation ) {
		bigint_shift_left_raw( temp->divisor.element, size, rotation );
	}

	/* divide by subtraction */
	for ( i = 0 ; i <= rotation ; i++ ) {
		if ( bigint_is_geq ( &temp->dividend, &temp->divisor ) ) {
			bigint_subtract ( &temp->divisor, &temp->dividend );
			bigint_set_bit( quotient->element, rotation-i, size );
		}
		bigint_ror ( &temp->divisor );
	}
}
#endif

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
	int rotation;
	int i;

	/* Sanity check */
	assert ( sizeof ( *temp ) == bigint_mod_multiply_tmp_len ( modulus ) );

	/* Perform multiplication */
	bigint_multiply ( multiplicand, multiplier, &temp->result );

	/* Rescale modulus to match result */
	bigint_grow ( modulus, &temp->modulus );
	rotation = ( bigint_max_set_bit ( &temp->result ) -
		     bigint_max_set_bit ( &temp->modulus ) );

#ifdef BIGINT_FAST_SHIFT
	if (0 < rotation) {
		bigint_shift_left_raw(temp->modulus.element, size * 2, rotation);
	}
#else
	for ( i = 0 ; i < rotation ; i++ )
		bigint_rol ( &temp->modulus );
#endif
	/* Subtract multiples of modulus */
	for ( i = 0 ; i <= rotation ; i++ ) {
		if ( bigint_is_geq ( &temp->result, &temp->modulus ) )
			bigint_subtract ( &temp->modulus, &temp->result );
		bigint_ror ( &temp->modulus );
	}

	/* Resize result */
	bigint_shrink ( &temp->result, result );

	/* Sanity check */
	assert ( bigint_is_geq ( modulus, result ) );
}

/**
 * Perform classic modular exponentiation of big integers
 *
 * @v base0		Element 0 of big integer base
 * @v modulus0		Element 0 of big integer modulus
 * @v exponent0		Element 0 of big integer exponent
 * @v result0		Element 0 of big integer to hold result
 * @v size		Number of elements in base, modulus, and result
 * @v exponent_size	Number of elements in exponent
 * @v tmp		Temporary working space
 */
static 
void bigint_mod_exp_classic_raw ( const bigint_element_t *base0,
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
	int i, exponent_high_bit;

	struct {
		bigint_t ( size ) base;
		bigint_t ( exponent_size ) exponent;
		uint8_t mod_multiply[mod_multiply_len];
	} *temp = tmp;
	static const uint8_t start[1] = { 0x01 };
/*
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
*/
        memcpy ( &temp->base, base, sizeof ( temp->base ) );
        bigint_init ( result, start, sizeof ( start ) );

        exponent_high_bit = bigint_max_set_bit ( exponent );

        for ( i = 0; i < exponent_high_bit; i++ ) {
                if ( bigint_bit_is_set ( exponent, i ) ) {
                        bigint_mod_multiply ( result, &temp->base, modulus,
                                              result, temp->mod_multiply );
                }
                bigint_mod_multiply ( &temp->base, &temp->base, modulus,
                                      &temp->base, temp->mod_multiply );
        }
}

#ifdef BIGINT_MONTGOMERY

#define MONTGOMERY_CONTEXT(size) struct { \
	unsigned int power;             \
	bigint_t ( size ) base;         \
	bigint_t ( size ) mont_base;    \
	bigint_t ( size ) modulus;      \
	bigint_t ( size ) result;       \
	bigint_t ( size ) R;            \
	bigint_t ( size ) rInv;         \
	bigint_t ( size ) mInv;         \
	bigint_t ( size * 2 ) t;        \
	bigint_t ( size * 2 ) p;        \
	bigint_t ( size * 2 ) q;        \
	bigint_t ( size ) u[3];         \
	bigint_t ( size ) v[3];         \
	bigint_t ( size ) w[3];         \
	uint8_t mod_multiply[4 * sizeof( bigint_t ( (size) * 2 ) )];    \
}

#define bigint_montgomery_product(mul0, mul1, modulus, result, tmp) do { \
unsigned int size = bigint_size(modulus); \
bigint_montgomery_product_raw ( (mul0)->element, (mul1)->element, \
(modulus)->element, (result)->element, size, (tmp)); \
} while (0)


/* Extended Euclid Algorithm */
static 
void bigint_invert ( const bigint_element_t *arg0,
			const bigint_element_t *modulus0,
			bigint_element_t *arg_inv0,
			bigint_element_t *mod_inv0,
			unsigned int size,
			void *tmp ) {

	MONTGOMERY_CONTEXT ( size ) *temp = tmp;
	bigint_element_t  *pu[3], *pv[3], *pw[3], *swap;
	unsigned int i;
	static const uint8_t one[1] = { 0x01 };
	static const uint8_t zero[1] = { 0x00 };

	for (i = 0; i < 3; i++) {
		pu[i] = temp->u[i].element;
		pv[i] = temp->v[i].element;
		pw[i] = temp->w[i].element;
	}

	bigint_init_raw ( pu[0], size, one, sizeof(one) );
	bigint_init_raw ( pu[1], size, zero, sizeof(zero) );
	bigint_shrink_raw ( modulus0, size, pu[2], size );

	bigint_init_raw ( pv[0], size, zero, sizeof(zero) );
	bigint_init_raw ( pv[1], size, one, sizeof(one) );
	bigint_shrink_raw ( arg0, size, pv[2], size );

	while ( ! bigint_is_zero_raw ( pv[2], size ) ) {
		/* q = u / v */
		bigint_div_raw ( pu[2], pv[2], temp->q.element, size, &temp->mod_multiply );
		for ( i = 3; i-- ; ) {
			/*  w[i] = u[i] - q * v[i] */
			bigint_multiply_raw( temp->q.element, pv[i], temp->p.element, size );
			bigint_shrink_raw ( pu[i], size, pw[i], size );
			bigint_subtract_raw ( temp->p.element,  pw[i], size );

			swap = pu[i];
			pu[i] = pv[i];
			pv[i] = pw[i];
			pw[i] = swap;
		}
	}
	/* output the result */
	bigint_shrink_raw(pu[0], size, mod_inv0, size);
	bigint_shrink_raw(pu[1], size, arg_inv0, size);
}

static 
void bigint_montgomery_init(const bigint_element_t *base0,
			const bigint_element_t *modulus0,
			bigint_element_t *result0,
			unsigned int size,
			void *tmp ) {
	const bigint_t ( size ) __attribute__ (( may_alias )) *base =
		( ( const void * ) base0 );
	const bigint_t ( size ) __attribute__ (( may_alias )) *modulus =
		( ( const void * ) modulus0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *result =
		( ( void * ) result0 );
	static const unsigned int bits_per_element = sizeof( bigint_element_t ) * 8;
	MONTGOMERY_CONTEXT ( size ) *temp = tmp;
	unsigned int  power;

	power = bigint_max_set_bit( modulus ) - 1;
	temp->power = ((power + bits_per_element)/bits_per_element) * bits_per_element;

	/* context->R = 2 ^ power */
	bigint_set_power2 ( temp->R.element, temp->power, size );

	bigint_invert ( temp->R.element, modulus->element, temp->rInv.element, temp->mInv.element, size, tmp );
	bigint_mod_multiply_raw ( temp->R.element, base->element, modulus->element, result->element, size, temp->mod_multiply );
}


static 
void bigint_montgomery_product_raw( const bigint_element_t *ma0,
				const bigint_element_t *mb0,
				const bigint_element_t *modulus0,
				bigint_element_t *result0,
				unsigned int size,
				void *tmp ) {
	const bigint_t ( size ) __attribute__ (( may_alias )) *ma =
		( ( const void * ) ma0 );
	const bigint_t ( size ) __attribute__ (( may_alias )) *mb =
		( ( const void * ) mb0 );
	const bigint_t ( size ) __attribute__ (( may_alias )) *modulus =
		( ( const void * ) modulus0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *result =
		( ( void * ) result0 );

	MONTGOMERY_CONTEXT ( size ) *temp = tmp;

	/* t = ma * mb mod R */
	bigint_multiply_raw ( ma->element, mb->element, temp->t.element, size );
	/* p = t * mInv mod R */
	bigint_multiply_raw ( temp->t.element, temp->mInv.element, temp->p.element, size );
	/*
	 * t = (p * m - t) / R,
	 * in the previous step we calculated (t * mInv) without (mod R)
	 * and we need to (mod R) here; modulus fits in (size-1), (mod R) is taking
	 * low (size-1) limbs, so we simply use (size-1) in the multiplication
	 */
	bigint_multiply_raw ( temp->p.element, modulus->element, temp->q.element, size-1 );

	if ( bigint_is_geq( &temp->q, &temp->t ) ) {
		bigint_subtract ( &temp->t, &temp->q );
		/* low (size) elements must be zero at this step */
		bigint_shift_right_raw ( temp->q.element, size * 2, temp->power );
		bigint_shrink ( modulus, result );
		bigint_subtract_raw ( temp->q.element, result->element, size );
	} else {
		bigint_subtract ( &temp->q, &temp->t );
		/* low (size) elements must be zero at this step */
		bigint_shift_right_raw ( temp->t.element, size * 2, temp->power );
		bigint_shrink ( &temp->t, result );
	}

	while ( bigint_is_geq( result, modulus )) {
		bigint_subtract ( modulus, result );
	}
}

/**
 * Perform  modular exponentiation of big integers using Montgomery method
 *
 * @v base0		Element 0 of big integer base
 * @v modulus0		Element 0 of big integer modulus
 * @v exponent0		Element 0 of big integer exponent
 * @v result0		Element 0 of big integer to hold result
 * @v size		Number of elements in base, modulus, and result
 * @v exponent_size	Number of elements in exponent
 */
static 
void bigint_mod_exp_montgomery_raw ( const bigint_element_t *base0,
				const bigint_element_t *modulus0,
				const bigint_element_t *exponent0,
				bigint_element_t *result0,
				unsigned int size, unsigned int exponent_size,
				void *tmp ) {
	const bigint_t ( size ) __attribute__ (( may_alias )) *base =
		( ( const void * ) base0 );
	const bigint_t ( size ) __attribute__ (( may_alias )) *modulus =
		( ( const void * ) modulus0 );
	const bigint_t ( exponent_size ) __attribute__ (( may_alias )) *exponent =
		( ( const void * ) exponent0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *result =
		( ( void * ) result0 );
	void __attribute__ (( unused )) *temp = tmp;

	static const uint8_t start[1] = { 0x01 };
	unsigned int  i, exponent_high_bit;
	MONTGOMERY_CONTEXT ( size+1 ) *context;
	exponent_high_bit = bigint_max_set_bit( exponent );
	if ( 0 == exponent_high_bit ) {
		bigint_init ( result, start, sizeof ( start ) );
		return;
	}

	context = malloc( sizeof ( *context ) );
	if ( NULL == context ) {
		return;
	}

	/* accumulate result in the context->result */
	bigint_init ( &context->result, start, sizeof ( start ) );
	/* expand numbers to (size+1) elements*/
	bigint_grow(base, &context->base);
	bigint_grow(modulus, &context->modulus);
	bigint_montgomery_init ( context->base.element, context->modulus.element, context->mont_base.element, size+1, context );
	/* result = R */
	bigint_shrink( &context->R, &context->result );

	for ( i = 0; i < exponent_high_bit; i++ ) {
		if ( bigint_bit_is_set ( exponent, i ) ) {
			bigint_montgomery_product ( &context->result, &context->mont_base, 
						&context->modulus, &context->result, context );
		}
		bigint_montgomery_product ( &context->mont_base, &context->mont_base, 
						&context->modulus, &context->mont_base, context );
	}

	if ( context->rInv.element[size] ) {
		/* (0 > R^-1); it means context-rInv = R - (R^-1) */
		context->rInv.element[size] = 0;
		bigint_mod_multiply_raw( context->result.element, context->rInv.element, context->modulus.element, 
					context->mont_base.element, size+1, &context->mod_multiply);
		bigint_mod_multiply_raw( context->result.element, context->R.element, context->modulus.element, 
					context->rInv.element, size+1, &context->mod_multiply);
		if ( bigint_is_geq( &context->rInv, &context->mont_base ) ) {
			bigint_add(&context->modulus, &context->mont_base);
		}
		bigint_subtract(&context->rInv, &context->mont_base);
	} else {
		bigint_mod_multiply_raw( context->result.element, context->rInv.element, context->modulus.element, 
					context->mont_base.element, size+1, &context->mod_multiply );
	}

	bigint_shrink( &context->mont_base, result );

	free ( context );
}
#endif

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
#ifdef BIGINT_MONTGOMERY
	bigint_mod_exp_montgomery_raw ( base0, modulus0, exponent0, result0, size, exponent_size );
#else
	bigint_mod_exp_classic_raw ( base0, modulus0, exponent0, result0, size, exponent_size, tmp );
#endif
}

