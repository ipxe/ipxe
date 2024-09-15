#ifndef _BITS_STRINGS_H
#define _BITS_STRINGS_H

/** @file
 *
 * String functions
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

extern __asmcall unsigned long riscv_ffs ( unsigned long value );
extern __asmcall unsigned long riscv_fls ( unsigned long value );

/**
 * Find first (i.e. least significant) set bit
 *
 * @v value		Value
 * @ret lsb		Least significant bit set in value (LSB=1), or zero
 */
static inline __attribute__ (( always_inline )) int __ffsl ( long value ) {

	return riscv_ffs ( value );
}

/**
 * Find first (i.e. least significant) set bit
 *
 * @v value		Value
 * @ret lsb		Least significant bit set in value (LSB=1), or zero
 */
static inline __attribute__ (( always_inline )) int __ffsll ( long long value ){
	unsigned long low = value;
	unsigned long high;

	/* Check machine word size */
	if ( sizeof ( value ) > sizeof ( low ) ) {
		/* 32-bit */
		high = ( value >> 32 );
		if ( low ) {
			return ( __ffsl ( low ) );
		} else if ( high ) {
			return ( 32 + __ffsl ( high ) );
		} else {
			return 0;
		}
	} else {
		/* 64-bit */
		return ( __ffsl ( low ) );
	}
}

/**
 * Find last (i.e. most significant) set bit
 *
 * @v value		Value
 * @ret msb		Most significant bit set in value (LSB=1), or zero
 */
static inline __attribute__ (( always_inline )) int __flsl ( long value ) {

	return riscv_fls ( value );
}

/**
 * Find last (i.e. most significant) set bit
 *
 * @v value		Value
 * @ret msb		Most significant bit set in value (LSB=1), or zero
 */
static inline __attribute__ (( always_inline )) int __flsll ( long long value ){
	unsigned long low = value;
	unsigned long high;

	/* Check machine word size */
	if ( sizeof ( value ) > sizeof ( low ) ) {
		/* 32-bit */
		high = ( value >> 32 );
		if ( high ) {
			return ( 32 + __flsl ( high ) );
		} else if ( low ) {
			return ( __flsl ( low ) );
		} else {
			return 0;
		}
	} else {
		/* 64-bit */
		return ( __flsl ( low ) );
	}
}

#endif /* _BITS_STRINGS_H */
