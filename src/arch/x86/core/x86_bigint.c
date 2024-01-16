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
#include <ipxe/bigint.h>

/** @file
 *
 * Big integer support
 */

/**
 * Multiply big integers
 *
 * @v multiplicand0	Element 0 of big integer to be multiplied
 * @v multiplicand_size	Number of elements in multiplicand
 * @v multiplier0	Element 0 of big integer to be multiplied
 * @v multiplier_size	Number of elements in multiplier
 * @v result0		Element 0 of big integer to hold result
 */
void bigint_multiply_raw ( const uint32_t *multiplicand0,
			   unsigned int multiplicand_size,
			   const uint32_t *multiplier0,
			   unsigned int multiplier_size,
			   uint32_t *result0 ) {
	unsigned int result_size = ( multiplicand_size + multiplier_size );
	const bigint_t ( multiplicand_size ) __attribute__ (( may_alias ))
		*multiplicand = ( ( const void * ) multiplicand0 );
	const bigint_t ( multiplier_size ) __attribute__ (( may_alias ))
		*multiplier = ( ( const void * ) multiplier0 );
	bigint_t ( result_size ) __attribute__ (( may_alias ))
		*result = ( ( void * ) result0 );
	unsigned int i;
	unsigned int j;
	uint32_t multiplicand_element;
	uint32_t multiplier_element;
	uint32_t *result_elements;
	uint32_t discard_a;
	uint32_t discard_d;
	long index;

	/* Zero result */
	memset ( result, 0, sizeof ( *result ) );

	/* Multiply integers one element at a time */
	for ( i = 0 ; i < multiplicand_size ; i++ ) {
		multiplicand_element = multiplicand->element[i];
		for ( j = 0 ; j < multiplier_size ; j++ ) {
			multiplier_element = multiplier->element[j];
			result_elements = &result->element[ i + j ];
			/* Perform a single multiply, and add the
			 * resulting double-element into the result,
			 * carrying as necessary.  The carry can
			 * never overflow beyond the end of the
			 * result, since:
			 *
			 *     a < 2^{n}, b < 2^{m} => ab < 2^{n+m}
			 */
			__asm__ __volatile__ ( "mull %5\n\t"
					       "addl %%eax, (%6,%2,4)\n\t"
					       "adcl %%edx, 4(%6,%2,4)\n\t"
					       "\n1:\n\t"
					       "adcl $0, 8(%6,%2,4)\n\t"
					       "inc %2\n\t"
						       /* Does not affect CF */
					       "jc 1b\n\t"
					       : "=&a" ( discard_a ),
						 "=&d" ( discard_d ),
						 "=&r" ( index ),
						 "+m" ( *result )
					       : "0" ( multiplicand_element ),
						 "g" ( multiplier_element ),
						 "r" ( result_elements ),
						 "2" ( 0 ) );
		}
	}
}
