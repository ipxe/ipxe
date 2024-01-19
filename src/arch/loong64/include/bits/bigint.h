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
typedef uint64_t bigint_element_t;

/**
 * Initialise big integer
 *
 * @v value0	Element 0 of big integer to initialise
 * @v size		Number of elements
 * @v data		Raw data
 * @v len		Length of raw data
 */
static inline __attribute__ (( always_inline )) void
bigint_init_raw ( uint64_t *value0, unsigned int size,
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
 */
static inline __attribute__ (( always_inline )) void
bigint_add_raw ( const uint64_t *addend0, uint64_t *value0,
		 unsigned int size ) {
	bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( void * ) value0 );
	uint64_t *discard_addend;
	uint64_t *discard_value;
	uint64_t discard_addend_i;
	uint64_t discard_value_i;
	uint64_t discard_carry;
	uint64_t discard_temp;
	unsigned int discard_size;

	__asm__ __volatile__ ( "\n1:\n\t"
			       /* Load addend[i] and value[i] */
			       "ld.d %3, %0, 0\n\t"
			       "ld.d %4, %1, 0\n\t"
			       /* Add carry flag and addend */
			       "add.d %4, %4, %5\n\t"
			       "sltu %6, %4, %5\n\t"
			       "add.d %4, %4, %3\n\t"
			       "sltu %5, %4, %3\n\t"
			       "or %5, %5, %6\n\t"
			       /* Store value[i] */
			       "st.d %4, %1, 0\n\t"
			       /* Loop */
			       "addi.d %0, %0, 8\n\t"
			       "addi.d %1, %1, 8\n\t"
			       "addi.w %2, %2, -1\n\t"
			       "bnez %2, 1b\n\t"
			       : "=r" ( discard_addend ),
				 "=r" ( discard_value ),
				 "=r" ( discard_size ),
				 "=r" ( discard_addend_i ),
				 "=r" ( discard_value_i ),
				 "=r" ( discard_carry ),
				 "=r" ( discard_temp ),
				 "+m" ( *value )
			       : "0" ( addend0 ), "1" ( value0 ),
				 "2" ( size ), "5" ( 0 ) );
}

/**
 * Subtract big integers
 *
 * @v subtrahend0	Element 0 of big integer to subtract
 * @v value0		Element 0 of big integer to be subtracted from
 * @v size		Number of elements
 */
static inline __attribute__ (( always_inline )) void
bigint_subtract_raw ( const uint64_t *subtrahend0, uint64_t *value0,
		      unsigned int size ) {
	bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( void * ) value0 );
	uint64_t *discard_subtrahend;
	uint64_t *discard_value;
	uint64_t discard_subtrahend_i;
	uint64_t discard_value_i;
	uint64_t discard_carry;
	uint64_t discard_temp;
	unsigned int discard_size;

	__asm__ __volatile__ ( "\n1:\n\t"
			       /* Load subtrahend[i] and value[i] */
			       "ld.d %3, %0, 0\n\t"
			       "ld.d %4, %1, 0\n\t"
			       /* Subtract carry flag and subtrahend */
			       "sltu %6, %4, %5\n\t"
			       "sub.d %4, %4, %5\n\t"
			       "sltu %5, %4, %3\n\t"
			       "sub.d %4, %4, %3\n\t"
			       "or %5, %5, %6\n\t"
			       /* Store value[i] */
			       "st.d %4, %1, 0\n\t"
			       /* Loop */
			       "addi.d %0, %0, 8\n\t"
			       "addi.d %1, %1, 8\n\t"
			       "addi.w %2, %2, -1\n\t"
			       "bnez %2, 1b\n\t"
			       : "=r" ( discard_subtrahend ),
				 "=r" ( discard_value ),
				 "=r" ( discard_size ),
				 "=r" ( discard_subtrahend_i ),
				 "=r" ( discard_value_i ),
				 "=r" ( discard_carry ),
				 "=r" ( discard_temp ),
				 "+m" ( *value )
			       : "0" ( subtrahend0 ), "1" ( value0 ),
				 "2" ( size ), "5" ( 0 ) );
}

/**
 * Rotate big integer left
 *
 * @v value0		Element 0 of big integer
 * @v size		Number of elements
 */
static inline __attribute__ (( always_inline )) void
bigint_rol_raw ( uint64_t *value0, unsigned int size ) {
	bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( void * ) value0 );
	uint64_t *discard_value;
	uint64_t discard_value_i;
	uint64_t discard_carry;
	uint64_t discard_temp;
	unsigned int discard_size;

	__asm__ __volatile__ ( "\n1:\n\t"
			       /* Load value[i] */
			       "ld.d %2, %0, 0\n\t"
			       /* Shift left */
			       "rotri.d %2, %2, 63\n\t"
			       "andi %4, %2, 1\n\t"
			       "xor %2, %2, %4\n\t"
			       "or %2, %2, %3\n\t"
			       "move %3, %4\n\t"
			       /* Store value[i] */
			       "st.d %2, %0, 0\n\t"
			       /* Loop  */
			       "addi.d %0, %0, 8\n\t"
			       "addi.w %1, %1, -1\n\t"
			       "bnez %1, 1b\n\t"
			       : "=r" ( discard_value ),
				 "=r" ( discard_size ),
				 "=r" ( discard_value_i ),
				 "=r" ( discard_carry ),
				 "=r" ( discard_temp ),
				 "+m" ( *value )
			       : "0" ( value0 ), "1" ( size ), "3" ( 0 )
			       : "cc" );
}

/**
 * Rotate big integer right
 *
 * @v value0		Element 0 of big integer
 * @v size		Number of elements
 */
static inline __attribute__ (( always_inline )) void
bigint_ror_raw ( uint64_t *value0, unsigned int size ) {
	bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( void * ) value0 );
	uint64_t *discard_value;
	uint64_t discard_value_i;
	uint64_t discard_carry;
	uint64_t discard_temp;
	unsigned int discard_size;

	__asm__ __volatile__ ( "\n1:\n\t"
			       /* Load value[i] */
			       "ld.d %2, %0, -8\n\t"
			       /* Shift right */
			       "andi %4, %2, 1\n\t"
			       "xor %2, %2, %4\n\t"
			       "or %2, %2, %3\n\t"
			       "move %3, %4\n\t"
			       "rotri.d %2, %2, 1\n\t"
			       /* Store value[i] */
			       "st.d %2, %0, -8\n\t"
			       /* Loop  */
			       "addi.d %0, %0, -8\n\t"
			       "addi.w %1, %1, -1\n\t"
			       "bnez %1, 1b\n\t"
			       : "=r" ( discard_value ),
				 "=r" ( discard_size ),
				 "=r" ( discard_value_i ),
				 "=r" ( discard_carry ),
				 "=r" ( discard_temp ),
				 "+m" ( *value )
			       : "0" ( value0 + size ), "1" ( size ), "3" ( 0 )
			       : "cc" );
}

/**
 * Test if big integer is equal to zero
 *
 * @v value0		Element 0 of big integer
 * @v size		Number of elements
 * @ret is_zero		Big integer is equal to zero
 */
static inline __attribute__ (( always_inline, pure )) int
bigint_is_zero_raw ( const uint64_t *value0, unsigned int size ) {
	const uint64_t *value = value0;
	uint64_t value_i;

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
bigint_is_geq_raw ( const uint64_t *value0, const uint64_t *reference0,
		    unsigned int size ) {
	const uint64_t *value = ( value0 + size );
	const uint64_t *reference = ( reference0 + size );
	uint64_t value_i;
	uint64_t reference_i;

	do {
		value_i = *(--value);
		reference_i = *(--reference);
		if ( value_i != reference_i )
			break;
	} while ( --size );

	return ( value_i >= reference_i );
}

/**
 * Test if bit is set in big integer
 *
 * @v value0		Element 0 of big integer
 * @v size		Number of elements
 * @v bit		Bit to test
 * @ret is_set		Bit is set
 */
static inline __attribute__ (( always_inline )) int
bigint_bit_is_set_raw ( const uint64_t *value0, unsigned int size,
			unsigned int bit ) {
	const bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( const void * ) value0 );
	unsigned int index = ( bit / ( 8 * sizeof ( value->element[0] ) ) );
	unsigned int subindex = ( bit % ( 8 * sizeof ( value->element[0] ) ) );

	return ( !! ( value->element[index] & ( 1UL << subindex ) ) );
}

/**
 * Find highest bit set in big integer
 *
 * @v value0		Element 0 of big integer
 * @v size		Number of elements
 * @ret max_bit		Highest bit set + 1 (or 0 if no bits set)
 */
static inline __attribute__ (( always_inline )) int
bigint_max_set_bit_raw ( const uint64_t *value0, unsigned int size ) {
	const uint64_t *value = ( value0 + size );
	int max_bit = ( 8 * sizeof ( bigint_t ( size ) ) );
	uint64_t value_i;

	do {
		value_i = *(--value);
		max_bit -= ( 64 - fls ( value_i ) );
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
bigint_grow_raw ( const uint64_t *source0, unsigned int source_size,
		  uint64_t *dest0, unsigned int dest_size ) {
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
bigint_shrink_raw ( const uint64_t *source0, unsigned int source_size __unused,
		    uint64_t *dest0, unsigned int dest_size ) {

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
bigint_done_raw ( const uint64_t *value0, unsigned int size __unused,
		  void *out, size_t len ) {
	const uint8_t *value_byte = ( ( const void * ) value0 );
	uint8_t *out_byte = ( out + len );

	/* Copy raw data in reverse order */
	while ( len-- )
		*(--out_byte) = *(value_byte++);
}

extern void bigint_multiply_raw ( const uint64_t *multiplicand0,
				  unsigned int multiplicand_size,
				  const uint64_t *multiplier0,
				  unsigned int multiplier_size,
				  uint64_t *value0 );

#endif /* _BITS_BIGINT_H */
