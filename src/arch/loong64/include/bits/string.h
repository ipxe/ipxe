#ifndef _BITS_STRING_H
#define _BITS_STRING_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

/** @file
 *
 * String functions
 *
 */

extern void loong64_bzero ( void *dest, size_t len );
extern void loong64_memset ( void *dest, size_t len, int character );
extern void loong64_memcpy ( void *dest, const void *src, size_t len );
extern void loong64_memmove ( void *dest, const void *src, size_t len );

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

	/* Zeroing: use the optimised variable-length zeroing code */
	if ( __builtin_constant_p ( character ) && ( character == 0 ) ) {
		loong64_bzero ( dest, len );
		return dest;
	}

	/* Not necessarily zeroing: use basic variable-length code */
	loong64_memset ( dest, len, character );
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

	loong64_memcpy ( dest, src, len );
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

	/* If direction of copy is known to be forwards at build time,
	 * then use variable-length memcpy().
	 */
	if ( __builtin_constant_p ( offset ) && ( offset <= 0 ) ) {
		loong64_memcpy ( dest, src, len );
		return dest;
	}

	/* Otherwise, use ambidirectional copy */
	loong64_memmove ( dest, src, len );
	return dest;
}

#endif /* _BITS_STRING_H */
