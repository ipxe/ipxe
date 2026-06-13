#ifndef _BITS_BIGINT_H
#define _BITS_BIGINT_H

/** @file
 *
 * Big integer support
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

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
	bigint_t ( size ) __attribute__ (( may_alias )) *addend =
		( ( void * ) addend0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( void * ) value0 );
	unsigned long discard_offset;
	unsigned long discard_temp;
	unsigned int index_carry;

	__asm__ ( /* Zero offset and clear carry-in */
		  "xgr %0, %0\n\t"
		  /* Add element by element */
		  "\n1:\n\t"
		  "lg %1, %O3(%0, %R3)\n\t"
		  "alcg %1, %O4(%0, %R4)\n\t"
		  "stg %1, %O3(%0, %R3)\n\t"
		  "la %0, 8(%0)\n\t"
		  "brct %2, 1b\n\t"
		  /* Obtain carry-out */
		  "alcr %2, %2\n\t"
		  : "=&a" ( discard_offset ),
		    "=&r" ( discard_temp ),
		    "=&r" ( index_carry ),
		    "+S" ( *value )
		  : "S" ( *addend ),
		    "2" ( size ) );

	return index_carry;
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
	bigint_t ( size ) __attribute__ (( may_alias )) *subtrahend =
		( ( void * ) subtrahend0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( void * ) value0 );
	unsigned long discard_offset;
	unsigned long discard_temp;
	unsigned int index_borrow;

	__asm__ ( /* Zero offset and clear (inverted) borrow-in */
		  "slgr %0, %0\n\t"
		  /* Subtract element by element */
		  "\n1:\n\t"
		  "lg %1, %O3(%0, %R3)\n\t"
		  "slbg %1, %O4(%0, %R4)\n\t"
		  "stg %1, %O3(%0, %R3)\n\t"
		  "la %0, 8(%0)\n\t"
		  "brct %2, 1b\n\t"
		  /* Obtain (negative) borrow-out */
		  "slbr %2, %2\n\t"
		  : "=&a" ( discard_offset ),
		    "=&r" ( discard_temp ),
		    "=&r" ( index_borrow ),
		    "+S" ( *value )
		  : "S" ( *subtrahend ),
		    "2" ( size ) );

	return ( -index_borrow );
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
	unsigned long discard_offset;
	unsigned long discard_temp;
	unsigned int discard_index;
	unsigned int carry;

	__asm__ ( /* Shift element by element */
		  "\n1:\n\t"
		  "lg %1, %O4(%0, %R4)\n\t"
		  "risbg %3, %1, 0, 62, 1\n\t"
		  "stg %3, %O4(%0, %R4)\n\t"
		  "srlg %3, %1, 63\n\t"
		  "la %0, 8(%0)\n\t"
		  "brct %2, 1b\n\t"
		  : "=&a" ( discard_offset ),
		    "=&r" ( discard_temp ),
		    "=&r" ( discard_index ),
		    "=&r" ( carry ),
		    "+S" ( *value )
		  : "0" ( 0UL ),
		    "2" ( size ),
		    "3" ( 0U ) );

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
	unsigned long discard_offset;
	unsigned long discard_temp;
	unsigned int carry;

	__asm__ ( /* Shift element by element */
		  "\n1:\n\t"
		  "sllg %1, %2, 63\n\t"
		  "lg %2, (%O3 - 8)(%0, %R3)\n\t"
		  "risbg %1, %2, 1, 63, 63\n\t"
		  "stg %1, (%O3 - 8)(%0, %R3)\n\t"
		  "aghi %0, -8\n\t"
		  "jnz 1b\n\t"
		  : "=&a" ( discard_offset ),
		    "=&r" ( discard_temp ),
		    "=&r" ( carry ),
		    "+S" ( *value )
		  : "0" ( sizeof ( *value ) ),
		    "2" ( 0 ) );

	return ( carry & 1 );
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
	const bigint_t ( source_size ) __attribute__ (( may_alias )) *source =
		( ( const void * ) source0 );
	bigint_t ( dest_size ) __attribute__ (( may_alias )) *dest =
		( ( void * ) dest0 );
	struct s390x_pointer_pair spair = { source, sizeof ( *source ) };
	struct s390x_pointer_pair dpair = { dest, sizeof ( *dest ) };

	__asm__ ( /* Copy source to destination, zero-padding as needed */
		  "\n1:\n\t"
		  "mvcle %0, %1, 0\n\t"
		  "jo 1b\n\t"
		  : "+r" ( dpair ), "+r" ( spair ), "=m" ( *dest )
		  : "m" ( *source ) );
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
bigint_shrink_raw ( const unsigned long *source0, unsigned int source_size,
		    unsigned long *dest0, unsigned int dest_size ) {
	const bigint_t ( source_size ) __attribute__ (( may_alias )) *source =
		( ( const void * ) source0 );
	bigint_t ( dest_size ) __attribute__ (( may_alias )) *dest =
		( ( void * ) dest0 );
	struct s390x_pointer_pair spair = { source, sizeof ( *source ) };
	struct s390x_pointer_pair dpair = { dest, sizeof ( *dest ) };

	__asm__ ( /* Copy source to destination, truncating if needed */
		  "\n1:\n\t"
		  "mvcle %0, %1, 0\n\t"
		  "jo 1b\n\t"
		  : "+r" ( dpair ), "+r" ( spair ), "=m" ( *dest )
		  : "m" ( *source ) );
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
	struct s390x_scalar_pair pair;

	__asm__ ( /* Perform multiplication */
		  "lgr %N0, %3\n\t"
		  "mlgr %0, %4\n\t"
		  /* Add carry-in */
		  "algr %N0, %2\n\t"
		  "slbgr %2, %2\n\t"
		  "aghi %2, 1\n\t"
		  /* Accumulate result */
		  "alg %N0, %1\n\t"
		  "stg %N0, %1\n\t"
		  /* Accumulate carry-out */
		  "alcgr %2, %0\n\t"
		  : "=&r" ( pair ),
		    "+T" ( *result ),
		    "+r" ( *carry )
		  : "r" ( multiplicand ),
		    "r" ( multiplier ) );
}

#endif /* _BITS_BIGINT_H */
