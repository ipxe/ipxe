#ifndef _BITS_STRING_H
#define _BITS_STRING_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * String functions
 *
 */

#include <stddef.h>

extern void s390x_memmove ( void *dest, const void *src, size_t len );

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
	const struct s390x_pointer_pair spair = { NULL, 0 };
	struct s390x_pointer_pair dpair = { dest, len };
	char ( * dmem ) [ len ] = dest;

	if ( __builtin_constant_p ( len ) && ( len == 0 ) ) {
		/* Constant zero length: do nothing */
	} else if ( __builtin_constant_p ( character ) && ( character == 0 ) &&
		    __builtin_constant_p ( len ) && ( len <= 256 ) ) {
		/* Constant small length, zeroing: use XOR-in-place */
		__asm__ ( "xc %O0(%1, %R0), %0"
			  : "=Q" ( *dmem )
			  : "i" ( len ) );
	} else if ( __builtin_constant_p ( character ) ) {
		/* Constant fill character: use "mvcle" with an immediate */
		__asm__ ( "\n1:\n\t"
			  "mvcle %0, %2, %3\n\t"
			  "jo 1b\n\t"
			  : "+r" ( dpair ), "=m" ( *dmem )
			  : "r" ( spair ), "i" ( character ) );
	} else {
		/* Variable fill character: use "mvcle" with a register */
		__asm__ ( "\n1:\n\t"
			  "mvcle %0, %2, 0(%3)\n\t"
			  "jo 1b\n\t"
			  : "+r" ( dpair ), "=m" ( *dmem )
			  : "r" ( spair ), "a" ( character ) );
	}

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
	struct s390x_pointer_pair spair = { src, len };
	struct s390x_pointer_pair dpair = { dest, len };
	const char ( * smem ) [ len ] = src;
	char ( * dmem ) [ len ] = dest;

	if ( __builtin_constant_p ( len ) && ( len == 0 ) ) {
		/* Constant zero length: do nothing */
	} else if ( __builtin_constant_p ( len ) && ( len <= 256 ) ) {
		/* Constant small non-zero length: use "mvc" */
		__asm__ ( "mvc %O0(%2,%R0), %1"
			  : "=Q" ( *dmem )
			  : "Q" ( *smem ), "i" ( len ) );
	} else {
		/* Variable or large length: use "mvcle" */
		__asm__ ( "\n1:\n\t"
			  "mvcle %0, %1, 0\n\t"
			  "jo 1b\n\t"
			  : "+r" ( dpair ), "+r" ( spair ), "=m" ( *dmem )
			  : "m" ( *smem ) );
	}

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
	 * then use standard memcpy().
	 */
	if ( __builtin_constant_p ( offset ) && ( offset <= 0 ) ) {
		memcpy ( dest, src, len );
		return dest;
	}

	/* Otherwise, use ambidirectional copy */
	s390x_memmove ( dest, src, len );
	return dest;
}

#endif /* _BITS_STRING_H */
