#ifndef _BITS_BITOPS_H
#define _BITS_BITOPS_H

/** @file
 *
 * loongArch bit operations
 *
 * We perform atomic bit set and bit clear operations using "ll"
 * and "sc".  We use the output constraint to inform the
 * compiler that any memory from the start of the bit field up to and
 * including the byte containing the bit may be modified.  (This is
 * overkill but shouldn't matter in practice since we're unlikely to
 * subsequently read other bits from the same bit field.)
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

/**
 * Test and set bit atomically
 *
 * @v bit		Bit to set
 * @v bits		Bit field
 * @ret old		Old value of bit (zero or non-zero)
 */
static inline __attribute__ (( always_inline )) int
test_and_set_bit ( unsigned int bit, volatile void *bits ) {
	unsigned int index = ( bit / 64 );
	unsigned int offset = ( bit % 64 );
	volatile uint64_t *qword = ( ( ( volatile uint64_t * ) bits ) + index );
	uint64_t mask = ( 1UL << offset );
	uint64_t old;
	uint64_t new;

	__asm__ __volatile__ ( "1:				\n\t"
			       "ll.d %[old], %[qword]		\n\t"
			       "or   %[new], %[old], %[mask]	\n\t"
			       "sc.d %[new], %[qword]		\n\t"
			       "beqz %[new], 1b			\n\t"
			       : [old] "=&r" ( old ),
				 [new] "=&r" ( new ),
				 [qword] "+m" ( *qword )
			       : [mask] "r" ( mask )
			       : "cc", "memory");
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
	unsigned int index = ( bit / 64 );
	unsigned int offset = ( bit % 64 );
	volatile uint64_t *qword = ( ( ( volatile uint64_t * ) bits ) + index );
	uint64_t mask = ( 1UL << offset );
	uint64_t old;
	uint64_t new;

	__asm__ __volatile__ ( "1:				\n\t"
			       "ll.d %[old], %[qword]		\n\t"
			       "andn %[new], %[old], %[mask]	\n\t"
			       "sc.d %[new], %[qword]		\n\t"
			       "beqz %[new], 1b			\n\t"
			       : [old] "=&r" ( old ),
				 [new] "=&r" ( new ),
				 [qword] "+m" ( *qword )
			       : [mask] "r" ( mask )
			       : "cc", "memory");
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
