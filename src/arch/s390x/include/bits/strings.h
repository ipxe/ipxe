#ifndef _BITS_STRINGS_H
#define _BITS_STRINGS_H

/** @file
 *
 * String functions
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/**
 * Find first (i.e. least significant) set bit
 *
 * @v value		Value
 * @ret lsb		Least significant bit set in value (LSB=1), or zero
 */
static inline __attribute__ (( always_inline )) int __ffsll ( long long value ){
	struct s390x_scalar_pair pair;

	/* Extract least significant set bit */
	value &= -value;

	/* Count number of leading zeros before LSB */
	__asm__ ( "flogr %0, %1" : "=r" ( pair ) : "r" ( value ) );

	return ( 64 - pair.even );
}

/**
 * Find first (i.e. least significant) set bit
 *
 * @v value		Value
 * @ret lsb		Least significant bit set in value (LSB=1), or zero
 */
static inline __attribute__ (( always_inline )) int __ffsl ( long value ) {

	return __ffsll ( value );
}

/**
 * Find last (i.e. most significant) set bit
 *
 * @v value		Value
 * @ret msb		Most significant bit set in value (LSB=1), or zero
 */
static inline __attribute__ (( always_inline )) int __flsll ( long long value ){
	struct s390x_scalar_pair pair;

	/* Count leading zeros */
	__asm__ ( "flogr %0, %1" : "=r" ( pair ) : "r" ( value ) );
	return ( 64 - pair.even );
}

/**
 * Find last (i.e. most significant) set bit
 *
 * @v value		Value
 * @ret msb		Most significant bit set in value (LSB=1), or zero
 */
static inline __attribute__ (( always_inline )) int __flsl ( long value ) {

	return __flsll ( value );
}

#endif /* _BITS_STRINGS_H */
