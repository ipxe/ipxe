#ifndef _BITS_BITOPS_H
#define _BITS_BITOPS_H

/** @file
 *
 * S/390 bit operations
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <byteswap.h>

/**
 * Test and set bit atomically
 *
 * @v bit		Bit to set
 * @v bits		Bit field
 * @ret old		Old value of bit (zero or non-zero)
 */
static inline __attribute__ (( always_inline )) int
test_and_set_bit ( unsigned int bit, volatile void *bits ) {
	unsigned int index = ( bit / 32 );
	unsigned int offset = ( bit % 32 );
	volatile uint32_t *word = ( ( ( volatile uint32_t * ) bits ) + index );
	uint32_t mask = cpu_to_le32 ( 1U << offset );
	uint32_t old;

	__asm__ __volatile__ ( "lao %0, %2, %1"
			       : "=r" ( old ), "+S" ( *word )
			       : "r" ( mask ) );

	return ( !! ( old & mask ) );
}

/**
 * Test and clear bit atomically
 *
 * @v bit		Bit to set
 * @v bits		Bit field
 * @ret old		Old value of bit (zero or non-zero)
 */
static inline __attribute__ (( always_inline )) int
test_and_clear_bit ( unsigned int bit, volatile void *bits ) {
	unsigned int index = ( bit / 32 );
	unsigned int offset = ( bit % 32 );
	volatile uint32_t *word = ( ( ( volatile uint32_t * ) bits ) + index );
	uint32_t mask = cpu_to_le32 ( 1U << offset );
	uint32_t old;

	__asm__ __volatile__ ( "lan %0, %2, %1"
			       : "=r" ( old ), "+S" ( *word )
			       : "r" ( ~mask ) );

	return ( !! ( old & mask ) );
}

/**
 * Set bit atomically
 *
 * @v bit		Bit to set
 * @v bits		Bit field
 */
static inline __attribute__ (( always_inline )) void
set_bit ( unsigned int bit, volatile void *bits ) {

	test_and_set_bit ( bit, bits );
}

/**
 * Clear bit atomically
 *
 * @v bit		Bit to set
 * @v bits		Bit field
 */
static inline __attribute__ (( always_inline )) void
clear_bit ( unsigned int bit, volatile void *bits ) {

	test_and_clear_bit ( bit, bits );
}

#endif /* _BITS_BITOPS_H */
