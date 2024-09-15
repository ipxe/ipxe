#ifndef _BITS_STRING_H
#define _BITS_STRING_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * String functions
 *
 */

extern void riscv_bzero ( void *dest, size_t len );
extern void riscv_memset ( void *dest, size_t len, int character );
extern void riscv_memcpy ( void *dest, const void *src, size_t len );
extern void riscv_memmove_forwards ( void *dest, const void *src, size_t len );
extern void riscv_memmove_backwards ( void *dest, const void *src, size_t len );
extern void riscv_memmove ( void *dest, const void *src, size_t len );

/**
 * Fill memory region
 *
 * @v dest		Destination region
 * @v character		Fill character
 * @v len		Length
 * @ret dest		Destination region
 */
static inline __attribute__ (( always_inline )) void *
memset ( void *dest, int character, size_t len ) {

	/* For zeroing larger or non-constant lengths, use the
	 * optimised variable-length zeroing code.
	 */
	if ( __builtin_constant_p ( character ) && ( character == 0 ) ) {
		riscv_bzero ( dest, len );
		return dest;
	}

	/* Not necessarily zeroing: use basic variable-length code */
	riscv_memset ( dest, len, character );
	return dest;
}

/**
 * Copy memory region
 *
 * @v dest		Destination region
 * @v src		Source region
 * @v len		Length
 * @ret dest		Destination region
 */
static inline __attribute__ (( always_inline )) void *
memcpy ( void *dest, const void *src, size_t len ) {

	/* Otherwise, use variable-length code */
	riscv_memcpy ( dest, src, len );
	return dest;
}

/**
 * Copy (possibly overlapping) memory region
 *
 * @v dest		Destination region
 * @v src		Source region
 * @v len		Length
 * @ret dest		Destination region
 */
static inline __attribute__ (( always_inline )) void *
memmove ( void *dest, const void *src, size_t len ) {
	ssize_t offset = ( dest - src );

	/* If required direction of copy is known at build time, then
	 * use the appropriate forwards/backwards copy directly.
	 */
	if ( __builtin_constant_p ( offset ) ) {
		if ( offset <= 0 ) {
			riscv_memmove_forwards ( dest, src, len );
			return dest;
		} else {
			riscv_memmove_backwards ( dest, src, len );
			return dest;
		}
	}

	/* Otherwise, use ambidirectional copy */
	riscv_memmove ( dest, src, len );
	return dest;
}

#endif /* _BITS_STRING_H */
