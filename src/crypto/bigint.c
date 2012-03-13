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

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <ipxe/bigint.h>

/** @file
 *
 * Big integer support
 */

/**
 * Perform modular multiplication of big integers
 *
 * @v multiplicand0	Element 0 of big integer to be multiplied
 * @v multiplier0	Element 0 of big integer to be multiplied
 * @v modulus0		Element 0 of big integer modulus
 * @v result0		Element 0 of big integer to hold result
 */
void bigint_mod_multiply_raw ( const bigint_element_t *multiplicand0,
			       const bigint_element_t *multiplier0,
			       const bigint_element_t *modulus0,
			       bigint_element_t *result0,
			       unsigned int size ) {
	const bigint_t ( size ) __attribute__ (( may_alias )) *multiplicand =
		( ( const void * ) multiplicand0 );
	const bigint_t ( size ) __attribute__ (( may_alias )) *multiplier =
		( ( const void * ) multiplier0 );
	const bigint_t ( size ) __attribute__ (( may_alias )) *modulus =
		( ( const void * ) modulus0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *result =
		( ( void * ) result0 );
	bigint_t ( size * 2 ) temp_result;
	bigint_t ( size * 2 ) temp_modulus;
	int rotation;
	int i;

	/* Perform multiplication */
	bigint_multiply ( multiplicand, multiplier, &temp_result );

	/* Rescale modulus to match result */
	bigint_grow ( modulus, &temp_modulus );
	rotation = ( bigint_max_set_bit ( &temp_result ) -
		     bigint_max_set_bit ( &temp_modulus ) );
	for ( i = 0 ; i < rotation ; i++ )
		bigint_rol ( &temp_modulus );

	/* Subtract multiples of modulus */
	for ( i = 0 ; i <= rotation ; i++ ) {
		if ( bigint_is_geq ( &temp_result, &temp_modulus ) )
			bigint_subtract ( &temp_modulus, &temp_result );
		bigint_ror ( &temp_modulus );
	}

	/* Resize result */
	bigint_shrink ( &temp_result, result );

	/* Sanity check */
	assert ( bigint_is_geq ( modulus, result ) );
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
 */
void bigint_mod_exp_raw ( const bigint_element_t *base0,
			  const bigint_element_t *modulus0,
			  const bigint_element_t *exponent0,
			  bigint_element_t *result0,
			  unsigned int size,
			  unsigned int exponent_size ) {
	const bigint_t ( size ) __attribute__ (( may_alias )) *base =
		( ( const void * ) base0 );
	const bigint_t ( size ) __attribute__ (( may_alias )) *modulus =
		( ( const void * ) modulus0 );
	const bigint_t ( exponent_size ) __attribute__ (( may_alias ))
		*exponent = ( ( const void * ) exponent0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *result =
		( ( void * ) result0 );
	bigint_t ( size ) temp_base;
	bigint_t ( exponent_size ) temp_exponent;
	static const uint8_t start[1] = { 0x01 };

	memcpy ( &temp_base, base, sizeof ( temp_base ) );
	memcpy ( &temp_exponent, exponent, sizeof ( temp_exponent ) );
	bigint_init ( result, start, sizeof ( start ) );

	while ( ! bigint_is_zero ( &temp_exponent ) ) {
		if ( bigint_bit_is_set ( &temp_exponent, 0 ) ) {
			bigint_mod_multiply ( result, &temp_base,
					      modulus, result );
		}
		bigint_ror ( &temp_exponent );
		bigint_mod_multiply ( &temp_base, &temp_base, modulus,
				      &temp_base );
	}
}
