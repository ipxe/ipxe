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
typedef uint64_t bigint_element_t;

/**
 * Add big integers
 *
 * @v addend0		Element 0 of big integer to add
 * @v value0		Element 0 of big integer to be added to
 * @v size		Number of elements
 * @ret carry		Carry out
 */
static inline __attribute__ (( always_inline )) int
bigint_add_raw ( const uint64_t *addend0, uint64_t *value0,
		 unsigned int size ) {
	bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( void * ) value0 );
	uint64_t *discard_addend;
	uint64_t *discard_value;
	uint64_t discard_addend_i;
	uint64_t discard_value_i;
	unsigned int discard_size;
	int carry;

	__asm__ __volatile__ ( "cmn xzr, xzr\n\t" /* clear CF */
			       "\n1:\n\t"
			       "ldr %3, [%0], #8\n\t"
			       "ldr %4, [%1]\n\t"
			       "adcs %4, %4, %3\n\t"
			       "str %4, [%1], #8\n\t"
			       "sub %w2, %w2, #1\n\t"
			       "cbnz %w2, 1b\n\t"
			       : "=r" ( discard_addend ),
				 "=r" ( discard_value ),
				 "=r" ( discard_size ),
				 "=r" ( discard_addend_i ),
				 "=r" ( discard_value_i ),
				 "=@cccs" ( carry ),
				 "+m" ( *value )
			       : "0" ( addend0 ), "1" ( value0 ),
				 "2" ( size ) );
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
bigint_subtract_raw ( const uint64_t *subtrahend0, uint64_t *value0,
		      unsigned int size ) {
	bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( void * ) value0 );
	uint64_t *discard_subtrahend;
	uint64_t *discard_value;
	uint64_t discard_subtrahend_i;
	uint64_t discard_value_i;
	unsigned int discard_size;
	int borrow;

	__asm__ __volatile__ ( "cmp xzr, xzr\n\t" /* set CF */
			       "\n1:\n\t"
			       "ldr %3, [%0], #8\n\t"
			       "ldr %4, [%1]\n\t"
			       "sbcs %4, %4, %3\n\t"
			       "str %4, [%1], #8\n\t"
			       "sub %w2, %w2, #1\n\t"
			       "cbnz %w2, 1b\n\t"
			       : "=r" ( discard_subtrahend ),
				 "=r" ( discard_value ),
				 "=r" ( discard_size ),
				 "=r" ( discard_subtrahend_i ),
				 "=r" ( discard_value_i ),
				 "=@cccc" ( borrow ),
				 "+m" ( *value )
			       : "0" ( subtrahend0 ), "1" ( value0 ),
				 "2" ( size ) );
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
bigint_shl_raw ( uint64_t *value0, unsigned int size ) {
	bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( void * ) value0 );
	uint64_t *discard_value;
	uint64_t discard_value_i;
	unsigned int discard_size;
	int carry;

	__asm__ __volatile__ ( "cmn xzr, xzr\n\t" /* clear CF */
			       "\n1:\n\t"
			       "ldr %2, [%0]\n\t"
			       "adcs %2, %2, %2\n\t"
			       "str %2, [%0], #8\n\t"
			       "sub %w1, %w1, #1\n\t"
			       "cbnz %w1, 1b\n\t"
			       : "=r" ( discard_value ),
				 "=r" ( discard_size ),
				 "=r" ( discard_value_i ),
				 "=@cccs" ( carry ),
				 "+m" ( *value )
			       : "0" ( value0 ), "1" ( size ) );
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
bigint_shr_raw ( uint64_t *value0, unsigned int size ) {
	bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( void * ) value0 );
	uint64_t *discard_value;
	uint64_t discard_high;
	unsigned int discard_size;
	uint64_t low;

	__asm__ __volatile__ ( "mov %2, #0\n\t"
			       "\n1:\n\t"
			       "sub %w1, %w1, #1\n\t"
			       "ldr %3, [%0, %1, lsl #3]\n\t"
			       "extr %2, %2, %3, #1\n\t"
			       "str %2, [%0, %1, lsl #3]\n\t"
			       "mov %2, %3\n\t"
			       "cbnz %w1, 1b\n\t"
			       : "=r" ( discard_value ),
				 "=r" ( discard_size ),
				 "=r" ( discard_high ),
				 "=r" ( low ),
				 "+m" ( *value )
			       : "0" ( value0 ), "1" ( size ) );
	return ( low & 1 );
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
 * Multiply big integer elements
 *
 * @v multiplicand	Multiplicand element
 * @v multiplier	Multiplier element
 * @v result		Result element
 * @v carry		Carry element
 */
static inline __attribute__ (( always_inline )) void
bigint_multiply_one ( const uint64_t multiplicand, const uint64_t multiplier,
		      uint64_t *result, uint64_t *carry ) {
	uint64_t discard_low;
	uint64_t discard_high;

	__asm__ __volatile__ ( /* Perform multiplication */
			       "mul %0, %4, %5\n\t"
			       "umulh %1, %4, %5\n\t"
			       /* Accumulate result */
			       "adds %2, %2, %0\n\t"
			       "adc %1, %1, xzr\n\t"
			       /* Accumulate carry (cannot overflow) */
			       "adds %2, %2, %3\n\t"
			       "adc %3, %1, xzr\n\t"
			       : "=&r" ( discard_low ),
				 "=r" ( discard_high ),
				 "+r" ( *result ),
				 "+r" ( *carry )
			       : "r" ( multiplicand ),
				 "r" ( multiplier )
			       : "cc" );
}

#endif /* _BITS_BIGINT_H */
