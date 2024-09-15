/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>
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
void bigint_multiply_raw ( const unsigned long *multiplicand0,
			   unsigned int multiplicand_size,
			   const unsigned long *multiplier0,
			   unsigned int multiplier_size,
			   unsigned long *result0 ) {
	unsigned int result_size = ( multiplicand_size + multiplier_size );
	const bigint_t ( multiplicand_size ) __attribute__ (( may_alias ))
		*multiplicand = ( ( const void * ) multiplicand0 );
	const bigint_t ( multiplier_size ) __attribute__ (( may_alias ))
		*multiplier = ( ( const void * ) multiplier0 );
	bigint_t ( result_size ) __attribute__ (( may_alias ))
		*result = ( ( void * ) result0 );
	unsigned int i;
	unsigned int j;
	unsigned long multiplicand_element;
	unsigned long multiplier_element;
	unsigned long *result_elements;
	unsigned long discard_low;
	unsigned long discard_high;
	unsigned long discard_temp;
	unsigned long discard_carry;

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
			__asm__ __volatile__ ( /* Perform multiplication */
					       "mulhu %2, %6, %7\n\t"
					       "mul %1, %6, %7\n\t"
					       /* Accumulate low half */
					       LOADN " %3, (%0)\n\t"
					       "add %3, %3, %1\n\t"
					       "sltu %4, %3, %1\n\t"
					       STOREN " %3, 0(%0)\n\t"
					       /* Carry into high half */
					       "add %4, %4, %2\n\t"
					       /* Propagate as necessary */
					       "\n1:\n\t"
					       "addi %0, %0, %8\n\t"
					       LOADN " %3, 0(%0)\n\t"
					       "add %3, %3, %4\n\t"
					       "sltu %4, %3, %4\n\t"
					       STOREN " %3, 0(%0)\n\t"
					       "bnez %4, 1b\n\t"
					       : "+r" ( result_elements ),
						 "=r" ( discard_low ),
						 "=r" ( discard_high ),
						 "=r" ( discard_temp ),
						 "=r" ( discard_carry ),
						 "+m" ( *result )
					       : "r" ( multiplicand_element ),
						 "r" ( multiplier_element ),
						 "i" ( sizeof ( *result0 ) ) );
		}
	}
}
