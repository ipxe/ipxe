#ifndef _BITS_BIGINT_H
#define _BITS_BIGINT_H

/** @file
 *
 * Big integer support
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <string.h>
#include <strings.h>

/** Element of a big integer */
typedef unsigned long bigint_element_t;

/**
 * Initialise big integer
 *
 * @v value0		Element 0 of big integer to initialise
 * @v size		Number of elements
 * @v data		Raw data
 * @v len		Length of raw data
 */
static inline __attribute__ (( always_inline )) void
bigint_init_raw ( unsigned long *value0, unsigned int size,
		  const void *data, size_t len ) {
	size_t pad_len = ( sizeof ( bigint_t ( size ) ) - len );
	uint8_t *value_byte = ( ( void * ) value0 );
	const uint8_t *data_byte = ( data + len );

	/* Copy raw data in reverse order, padding with zeros */
	while ( len-- )
		*(value_byte++) = *(--data_byte);
	while ( pad_len-- )
		*(value_byte++) = 0;
}

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
 * Test if big integer is equal to zero
 *
 * @v value0		Element 0 of big integer
 * @v size		Number of elements
 * @ret is_zero		Big integer is equal to zero
 */
static inline __attribute__ (( always_inline, pure )) int
bigint_is_zero_raw ( const unsigned long *value0, unsigned int size ) {
	const unsigned long *value = value0;
	unsigned long value_i;

	do {
		value_i = *(value++);
		if ( value_i )
			break;
	} while ( --size );

	return ( value_i == 0 );
}

/**
 * Compare big integers
 *
 * @v value0		Element 0 of big integer
 * @v reference0	Element 0 of reference big integer
 * @v size		Number of elements
 * @ret geq		Big integer is greater than or equal to the reference
 */
static inline __attribute__ (( always_inline, pure )) int
bigint_is_geq_raw ( const unsigned long *value0,
		    const unsigned long *reference0, unsigned int size ) {
	const unsigned long *value = ( value0 + size );
	const unsigned long *reference = ( reference0 + size );
	unsigned long value_i;
	unsigned long reference_i;

	do {
		value_i = *(--value);
		reference_i = *(--reference);
		if ( value_i != reference_i )
			break;
	} while ( --size );

	return ( value_i >= reference_i );
}

/**
 * Find highest bit set in big integer
 *
 * @v value0		Element 0 of big integer
 * @v size		Number of elements
 * @ret max_bit		Highest bit set + 1 (or 0 if no bits set)
 */
static inline __attribute__ (( always_inline )) int
bigint_max_set_bit_raw ( const unsigned long *value0, unsigned int size ) {
	const unsigned long *value = ( value0 + size );
	int max_bit = ( 8 * sizeof ( bigint_t ( size ) ) );
	unsigned long value_i;

	do {
		value_i = *(--value);
		max_bit -= ( ( 8 * sizeof ( *value0 ) ) - fls ( value_i ) );
		if ( value_i )
			break;
	} while ( --size );

	return max_bit;
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
 * Finalise big integer
 *
 * @v value0		Element 0 of big integer to finalise
 * @v size		Number of elements
 * @v out		Output buffer
 * @v len		Length of output buffer
 */
static inline __attribute__ (( always_inline )) void
bigint_done_raw ( const unsigned long *value0, unsigned int size __unused,
		  void *out, size_t len ) {
	const uint8_t *value_byte = ( ( const void * ) value0 );
	uint8_t *out_byte = ( out + len );

	/* Copy raw data in reverse order */
	while ( len-- )
		*(--out_byte) = *(value_byte++);
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
