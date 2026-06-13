#ifndef _BITS_BIGINT_H
#define _BITS_BIGINT_H

/** @file
 *
 * Big integer support
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>
#include <string.h>

/** Element of a big integer */
typedef unsigned long bigint_element_t;

/**
 * Add big integers
 *
 * @v addend0		Element 0 of big integer to add
 * @v value0		Element 0 of big integer to be added to
 * @v size		Number of elements
 * @ret carry		Carry out
 */
static inline __attribute__ (( always_inline )) int
bigint_add_raw ( const unsigned long *addend0, unsigned long *value0,
		 unsigned int size ) {
	bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( void * ) value0 );
	unsigned long *valueN = ( value0 + size );
	unsigned long *discard_addend;
	unsigned long *discard_value;
	unsigned long discard_addend_i;
	unsigned long discard_value_i;
	unsigned long discard_temp;
	unsigned long carry;

	__asm__ __volatile__ ( "\n1:\n\t"
			       /* Load addend[i] and value[i] */
			       LOADN " %2, (%0)\n\t"
			       LOADN " %3, (%1)\n\t"
			       /* Add carry flag and addend */
			       "add %3, %3, %5\n\t"
			       "sltu %4, %3, %5\n\t"
			       "add %3, %3, %2\n\t"
			       "sltu %5, %3, %2\n\t"
			       "or %5, %4, %5\n\t"
			       /* Store value[i] */
			       STOREN " %3, (%1)\n\t"
			       /* Loop */
			       "addi %0, %0, %8\n\t"
			       "addi %1, %1, %8\n\t"
			       "bne %1, %7, 1b\n\t"
			       : "=&r" ( discard_addend ),
				 "=&r" ( discard_value ),
				 "=&r" ( discard_addend_i ),
				 "=&r" ( discard_value_i ),
				 "=&r" ( discard_temp ),
				 "=&r" ( carry ),
				 "+m" ( *value )
			       : "r" ( valueN ),
				 "i" ( sizeof ( unsigned long ) ),
				 "0" ( addend0 ), "1" ( value0 ), "5" ( 0 ) );
	return carry;
}

/**
 * Subtract big integers
 *
 * @v subtrahend0	Element 0 of big integer to subtract
 * @v value0		Element 0 of big integer to be subtracted from
 * @v size		Number of elements
 * @ret borrow		Borrow out
 */
static inline __attribute__ (( always_inline )) int
bigint_subtract_raw ( const unsigned long *subtrahend0, unsigned long *value0,
		      unsigned int size ) {
	bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( void * ) value0 );
	unsigned long *valueN = ( value0 + size );
	unsigned long *discard_subtrahend;
	unsigned long *discard_value;
	unsigned long discard_subtrahend_i;
	unsigned long discard_value_i;
	unsigned long discard_temp;
	unsigned long borrow;

	__asm__ __volatile__ ( "\n1:\n\t"
			       /* Load subtrahend[i] and value[i] */
			       LOADN " %2, (%0)\n\t"
			       LOADN " %3, (%1)\n\t"
			       /* Subtract carry flag and subtrahend */
			       "sltu %4, %3, %5\n\t"
			       "sub %3, %3, %5\n\t"
			       "sltu %5, %3, %2\n\t"
			       "sub %3, %3, %2\n\t"
			       "or %5, %5, %4\n\t"
			       /* Store value[i] */
			       STOREN " %3, (%1)\n\t"
			       /* Loop */
			       "addi %0, %0, %8\n\t"
			       "addi %1, %1, %8\n\t"
			       "bne %1, %7, 1b\n\t"
			       : "=&r" ( discard_subtrahend ),
				 "=&r" ( discard_value ),
				 "=&r" ( discard_subtrahend_i ),
				 "=&r" ( discard_value_i ),
				 "=&r" ( discard_temp ),
				 "=&r" ( borrow ),
				 "+m" ( *value )
			       : "r" ( valueN ),
				 "i" ( sizeof ( unsigned long ) ),
				 "0" ( subtrahend0 ), "1" ( value0 ),
				 "5" ( 0 ) );
	return borrow;
}

/**
 * Shift big integer left
 *
 * @v value0		Element 0 of big integer
 * @v size		Number of elements
 * @ret out		Bit shifted out
 */
static inline __attribute__ (( always_inline )) int
bigint_shl_raw ( unsigned long *value0, unsigned int size ) {
	bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( void * ) value0 );
	unsigned long *valueN = ( value0 + size );
	unsigned long *discard_value;
	unsigned long discard_value_i;
	unsigned long discard_temp;
	unsigned long carry;

	__asm__ __volatile__ ( "\n1:\n\t"
			       /* Load value[i] */
			       LOADN " %1, (%0)\n\t"
			       /* Shift left */
			       "slli %2, %1, 1\n\t"
			       "or %2, %2, %3\n\t"
			       "srli %3, %1, %7\n\t"
			       /* Store value[i] */
			       STOREN " %2, (%0)\n\t"
			       /* Loop  */
			       "addi %0, %0, %6\n\t"
			       "bne %0, %5, 1b\n\t"
			       : "=&r" ( discard_value ),
				 "=&r" ( discard_value_i ),
				 "=&r" ( discard_temp ),
				 "=&r" ( carry ),
				 "+m" ( *value )
			       : "r" ( valueN ),
				 "i" ( sizeof ( unsigned long ) ),
				 "i" ( ( 8 * sizeof ( unsigned long ) - 1 ) ),
				 "0" ( value0 ), "3" ( 0 ) );
	return carry;
}

/**
 * Shift big integer right
 *
 * @v value0		Element 0 of big integer
 * @v size		Number of elements
 * @ret out		Bit shifted out
 */
static inline __attribute__ (( always_inline )) int
bigint_shr_raw ( unsigned long *value0, unsigned int size ) {
	bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( void * ) value0 );
	unsigned long *valueN = ( value0 + size );
	unsigned long *discard_value;
	unsigned long discard_value_i;
	unsigned long discard_temp;
	unsigned long carry;

	__asm__ __volatile__ ( "\n1:\n\t"
			       /* Load value[i] */
			       LOADN " %1, %6(%0)\n\t"
			       /* Shift right */
			       "srli %2, %1, 1\n\t"
			       "or %2, %2, %3\n\t"
			       "slli %3, %1, %7\n\t"
			       /* Store value[i] */
			       STOREN " %2, %6(%0)\n\t"
			       /* Loop  */
			       "addi %0, %0, %6\n\t"
			       "bne %0, %5, 1b\n\t"
			       : "=&r" ( discard_value ),
				 "=&r" ( discard_value_i ),
				 "=&r" ( discard_temp ),
				 "=&r" ( carry ),
				 "+m" ( *value )
			       : "r" ( value0 ),
				 "i" ( -( sizeof ( unsigned long ) ) ),
				 "i" ( ( 8 * sizeof ( unsigned long ) - 1 ) ),
				 "0" ( valueN ), "3" ( 0 ) );
	return ( !! carry );
}

/**
 * Grow big integer
 *
 * @v source0		Element 0 of source big integer
 * @v source_size	Number of elements in source big integer
 * @v dest0		Element 0 of destination big integer
 * @v dest_size		Number of elements in destination big integer
 */
static inline __attribute__ (( always_inline )) void
bigint_grow_raw ( const unsigned long *source0, unsigned int source_size,
		  unsigned long *dest0, unsigned int dest_size ) {
	unsigned int pad_size = ( dest_size - source_size );

	memcpy ( dest0, source0, sizeof ( bigint_t ( source_size ) ) );
	memset ( ( dest0 + source_size ), 0, sizeof ( bigint_t ( pad_size ) ) );
}

/**
 * Shrink big integer
 *
 * @v source0		Element 0 of source big integer
 * @v source_size	Number of elements in source big integer
 * @v dest0		Element 0 of destination big integer
 * @v dest_size		Number of elements in destination big integer
 */
static inline __attribute__ (( always_inline )) void
bigint_shrink_raw ( const unsigned long *source0,
		    unsigned int source_size __unused,
		    unsigned long *dest0, unsigned int dest_size ) {

	memcpy ( dest0, source0, sizeof ( bigint_t ( dest_size ) ) );
}

/**
 * Multiply big integer elements
 *
 * @v multiplicand	Multiplicand element
 * @v multiplier	Multiplier element
 * @v result		Result element
 * @v carry		Carry element
 */
static inline __attribute__ (( always_inline )) void
bigint_multiply_one ( const unsigned long multiplicand,
		      const unsigned long multiplier,
		      unsigned long *result, unsigned long *carry ) {
	unsigned long discard_low;
	unsigned long discard_high;
	unsigned long discard_carry;

	__asm__ __volatile__ ( /* Perform multiplication */
			       "mulhu %1, %5, %6\n\t"
			       "mul %0, %5, %6\n\t"
			       /* Accumulate low half */
			       "add %3, %3, %0\n\t"
			       "sltu %2, %3, %0\n\t"
			       "add %1, %1, %2\n\t"
			       /* Accumulate carry (cannot overflow) */
			       "add %3, %3, %4\n\t"
			       "sltu %2, %3, %4\n\t"
			       "add %4, %1, %2\n\t"
			       : "=r" ( discard_low ),
				 "=&r" ( discard_high ),
				 "=r" ( discard_carry ),
				 "+r" ( *result ),
				 "+r" ( *carry )
			       : "r" ( multiplicand ),
				 "r" ( multiplier ) );
}

#endif /* _BITS_BIGINT_H */
