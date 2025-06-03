#ifndef _BITS_BIGINT_H
#define _BITS_BIGINT_H

/** @file
 *
 * Big integer support
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <string.h>

/** Element of a big integer */
typedef uint32_t bigint_element_t;

/**
 * Initialise big integer
 *
 * @v value0		Element 0 of big integer to initialise
 * @v size		Number of elements
 * @v data		Raw data
 * @v len		Length of raw data
 */
static inline __attribute__ (( always_inline )) void
bigint_init_raw ( uint32_t *value0, unsigned int size,
		  const void *data, size_t len ) {
	bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( void * ) value0 );
	long pad_len = ( sizeof ( *value ) - len );
	void *discard_D;
	long discard_c;

	/* Copy raw data in reverse order, padding with zeros */
	__asm__ __volatile__ ( "\n1:\n\t"
			       "movb -1(%3,%1), %%al\n\t"
			       "stosb\n\t"
			       "loop 1b\n\t"
			       "xorl %%eax, %%eax\n\t"
			       "mov %4, %1\n\t"
			       "rep stosb\n\t"
			       : "=&D" ( discard_D ), "=&c" ( discard_c ),
				 "+m" ( *value )
			       : "r" ( data ), "g" ( pad_len ), "0" ( value0 ),
				 "1" ( len )
			       : "eax" );
}

/**
 * Add big integers
 *
 * @v addend0		Element 0 of big integer to add
 * @v value0		Element 0 of big integer to be added to
 * @v size		Number of elements
 * @ret carry		Carry flag
 */
static inline __attribute__ (( always_inline )) int
bigint_add_raw ( const uint32_t *addend0, uint32_t *value0,
		 unsigned int size ) {
	bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( void * ) value0 );
	long index;
	void *discard_S;
	long discard_c;
	int carry;

	__asm__ __volatile__ ( "xor %0, %0\n\t" /* Zero %0 and clear CF */
			       "\n1:\n\t"
			       "lodsl\n\t"
			       "adcl %%eax, (%5,%0,4)\n\t"
			       "inc %0\n\t" /* Does not affect CF */
			       "loop 1b\n\t"
			       : "=&r" ( index ), "=&S" ( discard_S ),
				 "=&c" ( discard_c ), "=@ccc" ( carry ),
				 "+m" ( *value )
			       : "r" ( value0 ), "1" ( addend0 ), "2" ( size )
			       : "eax" );
	return carry;
}

/**
 * Subtract big integers
 *
 * @v subtrahend0	Element 0 of big integer to subtract
 * @v value0		Element 0 of big integer to be subtracted from
 * @v size		Number of elements
 * @ret borrow		Borrow flag
 */
static inline __attribute__ (( always_inline )) int
bigint_subtract_raw ( const uint32_t *subtrahend0, uint32_t *value0,
		      unsigned int size ) {
	bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( void * ) value0 );
	long index;
	void *discard_S;
	long discard_c;
	int borrow;

	__asm__ __volatile__ ( "xor %0, %0\n\t" /* Zero %0 and clear CF */
			       "\n1:\n\t"
			       "lodsl\n\t"
			       "sbbl %%eax, (%5,%0,4)\n\t"
			       "inc %0\n\t" /* Does not affect CF */
			       "loop 1b\n\t"
			       : "=&r" ( index ), "=&S" ( discard_S ),
				 "=&c" ( discard_c ), "=@ccc" ( borrow ),
				 "+m" ( *value )
			       : "r" ( value0 ), "1" ( subtrahend0 ),
				 "2" ( size )
			       : "eax" );
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
bigint_shl_raw ( uint32_t *value0, unsigned int size ) {
	bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( void * ) value0 );
	long index;
	long discard_c;
	int out;

	__asm__ __volatile__ ( "xor %0, %0\n\t" /* Zero %0 and clear CF */
			       "\n1:\n\t"
			       "rcll $1, (%4,%0,4)\n\t"
			       "inc %0\n\t" /* Does not affect CF */
			       "loop 1b\n\t"
			       : "=&r" ( index ), "=&c" ( discard_c ),
				 "=@ccc" ( out ), "+m" ( *value )
			       : "r" ( value0 ), "1" ( size ) );
	return out;
}

/**
 * Shift big integer right
 *
 * @v value0		Element 0 of big integer
 * @v size		Number of elements
 * @ret out		Bit shifted out
 */
static inline __attribute__ (( always_inline )) int
bigint_shr_raw ( uint32_t *value0, unsigned int size ) {
	bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( void * ) value0 );
	long discard_c;
	int out;

	__asm__ __volatile__ ( "clc\n\t"
			       "\n1:\n\t"
			       "rcrl $1, -4(%3,%0,4)\n\t"
			       "loop 1b\n\t"
			       : "=&c" ( discard_c ), "=@ccc" ( out ),
				 "+m" ( *value )
			       : "r" ( value0 ), "0" ( size ) );
	return out;
}

/**
 * Test if big integer is equal to zero
 *
 * @v value0		Element 0 of big integer
 * @v size		Number of elements
 * @ret is_zero		Big integer is equal to zero
 */
static inline __attribute__ (( always_inline, pure )) int
bigint_is_zero_raw ( const uint32_t *value0, unsigned int size ) {
	void *discard_D;
	long discard_c;
	int result;

	__asm__ __volatile__ ( "xor %0, %0\n\t" /* Set ZF */
			       "repe scasl\n\t"
			       "sete %b0\n\t"
			       : "=&a" ( result ), "=&D" ( discard_D ),
				 "=&c" ( discard_c )
			       : "1" ( value0 ), "2" ( size ) );
	return result;
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
bigint_is_geq_raw ( const uint32_t *value0, const uint32_t *reference0,
		    unsigned int size ) {
	long discard_c;
	long discard_tmp;
	int result;

	__asm__ __volatile__ ( "\n1:\n\t"
			       "movl -4(%3, %1, 4), %k2\n\t"
			       "cmpl -4(%4, %1, 4), %k2\n\t"
			       "loope 1b\n\t"
			       "setae %b0\n\t"
			       : "=q" ( result ), "=&c" ( discard_c ),
				 "=&r" ( discard_tmp )
			       : "r" ( value0 ), "r" ( reference0 ),
				 "0" ( 0 ), "1" ( size ) );
	return result;
}

/**
 * Find highest bit set in big integer
 *
 * @v value0		Element 0 of big integer
 * @v size		Number of elements
 * @ret max_bit		Highest bit set + 1 (or 0 if no bits set)
 */
static inline __attribute__ (( always_inline )) int
bigint_max_set_bit_raw ( const uint32_t *value0, unsigned int size ) {
	long discard_c;
	int result;

	__asm__ __volatile__ ( "\n1:\n\t"
			       "bsrl -4(%2,%1,4), %0\n\t"
			       "loopz 1b\n\t"
			       "rol %1\n\t" /* Does not affect ZF */
			       "rol %1\n\t"
			       "leal 1(%k0,%k1,8), %k0\n\t"
			       "jnz 2f\n\t"
			       "xor %0, %0\n\t"
			       "\n2:\n\t"
			       : "=&r" ( result ), "=&c" ( discard_c )
			       : "r" ( value0 ), "1" ( size ) );
	return result;
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
bigint_grow_raw ( const uint32_t *source0, unsigned int source_size,
		  uint32_t *dest0, unsigned int dest_size ) {
	bigint_t ( dest_size ) __attribute__ (( may_alias )) *dest =
		( ( void * ) dest0 );
	long pad_size = ( dest_size - source_size );
	void *discard_D;
	void *discard_S;
	long discard_c;

	__asm__ __volatile__ ( "rep movsl\n\t"
			       "xorl %%eax, %%eax\n\t"
			       "mov %4, %2\n\t"
			       "rep stosl\n\t"
			       : "=&D" ( discard_D ), "=&S" ( discard_S ),
				 "=&c" ( discard_c ), "+m" ( *dest )
			       : "g" ( pad_size ), "0" ( dest0 ),
				 "1" ( source0 ), "2" ( source_size )
			       : "eax" );
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
bigint_shrink_raw ( const uint32_t *source0, unsigned int source_size __unused,
		    uint32_t *dest0, unsigned int dest_size ) {
	bigint_t ( dest_size ) __attribute__ (( may_alias )) *dest =
		( ( void * ) dest0 );
	void *discard_D;
	void *discard_S;
	long discard_c;

	__asm__ __volatile__ ( "rep movsl\n\t"
			       : "=&D" ( discard_D ), "=&S" ( discard_S ),
				 "=&c" ( discard_c ), "+m" ( *dest )
			       : "0" ( dest0 ), "1" ( source0 ),
				 "2" ( dest_size )
			       : "eax" );
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
bigint_done_raw ( const uint32_t *value0, unsigned int size __unused,
		  void *out, size_t len ) {
	struct {
		uint8_t bytes[len];
	} __attribute__ (( may_alias )) *out_bytes = out;
	void *discard_D;
	long discard_c;

	/* Copy raw data in reverse order */
	__asm__ __volatile__ ( "\n1:\n\t"
			       "movb -1(%3,%1), %%al\n\t"
			       "stosb\n\t"
			       "loop 1b\n\t"
			       : "=&D" ( discard_D ), "=&c" ( discard_c ),
				 "+m" ( *out_bytes )
			       : "r" ( value0 ), "0" ( out ), "1" ( len )
			       : "eax" );
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
bigint_multiply_one ( const uint32_t multiplicand, const uint32_t multiplier,
		      uint32_t *result, uint32_t *carry ) {
	uint32_t discard_a;

	__asm__ __volatile__ ( /* Perform multiplication */
			       "mull %3\n\t"
			       /* Accumulate carry */
			       "addl %5, %0\n\t"
			       "adcl $0, %1\n\t"
			       /* Accumulate result */
			       "addl %0, %2\n\t"
			       "adcl $0, %1\n\t"
			       : "=&a" ( discard_a ),
				 "=&d" ( *carry ),
				 "+m" ( *result )
			       : "g" ( multiplicand ),
				 "0" ( multiplier ),
				 "r" ( *carry ) );
}

#endif /* _BITS_BIGINT_H */
