#ifndef ETHERBOOT_BITS_STRING_H
#define ETHERBOOT_BITS_STRING_H
/*
 * Taken from Linux /usr/include/asm/string.h
 * All except memcpy, memmove, memset and memcmp removed.
 *
 * Non-standard memswap() function added because it saves quite a bit
 * of code (mbrown@fensystems.co.uk).
 */

/*
 * This string-include defines all string functions as inline
 * functions. Use gcc. It also assumes ds=es=data space, this should be
 * normal. Most of the string-functions are rather heavily hand-optimized,
 * see especially strtok,strstr,str[c]spn. They should work, but are not
 * very easy to understand. Everything is done entirely within the register
 * set, making the functions fast and clean. String instructions have been
 * used through-out, making for "slightly" unclear code :-)
 *
 *		NO Copyright (C) 1991, 1992 Linus Torvalds,
 *		consider these trivial functions to be PD.
 */

FILE_LICENCE ( PUBLIC_DOMAIN );

#define __HAVE_ARCH_MEMCPY

extern void * __memcpy ( void *dest, const void *src, size_t len );
extern void * __memcpy_reverse ( void *dest, const void *src, size_t len );

/**
 * Copy memory area (where length is a compile-time constant)
 *
 * @v dest		Destination address
 * @v src		Source address
 * @v len		Length
 * @ret dest		Destination address
 */
static inline __attribute__ (( always_inline )) void *
__constant_memcpy ( void *dest, const void *src, size_t len ) {
	union {
		uint32_t u32[2];
		uint16_t u16[4];
		uint8_t  u8[8];
	} __attribute__ (( __may_alias__ )) *dest_u = dest;
	const union {
		uint32_t u32[2];
		uint16_t u16[4];
		uint8_t  u8[8];
	} __attribute__ (( __may_alias__ )) *src_u = src;
	const void *esi;
	void *edi;

	switch ( len ) {
	case 0 : /* 0 bytes */
		return dest;
	/*
	 * Single-register moves; these are always better than a
	 * string operation.  We can clobber an arbitrary two
	 * registers (data, source, dest can re-use source register)
	 * instead of being restricted to esi and edi.  There's also a
	 * much greater potential for optimising with nearby code.
	 *
	 */
	case 1 : /* 4 bytes */
		dest_u->u8[0]  = src_u->u8[0];
		return dest;
	case 2 : /* 6 bytes */
		dest_u->u16[0] = src_u->u16[0];
		return dest;
	case 4 : /* 4 bytes */
		dest_u->u32[0] = src_u->u32[0];
		return dest;
	/*
	 * Double-register moves; these are probably still a win.
	 *
	 */
	case 3 : /* 12 bytes */
		dest_u->u16[0] = src_u->u16[0];
		dest_u->u8[2]  = src_u->u8[2];
		return dest;
	case 5 : /* 10 bytes */
		dest_u->u32[0] = src_u->u32[0];
		dest_u->u8[4]  = src_u->u8[4];
		return dest;
	case 6 : /* 12 bytes */
		dest_u->u32[0] = src_u->u32[0];
		dest_u->u16[2] = src_u->u16[2];
		return dest;
	case 8 : /* 10 bytes */
		dest_u->u32[0] = src_u->u32[0];
		dest_u->u32[1] = src_u->u32[1];
		return dest;
	}

	/* Even if we have to load up esi and edi ready for a string
	 * operation, we can sometimes save space by using multiple
	 * single-byte "movs" operations instead of loading up ecx and
	 * using "rep movsb".
	 *
	 * "load ecx, rep movsb" is 7 bytes, plus an average of 1 byte
	 * to allow for saving/restoring ecx 50% of the time.
	 *
	 * "movsl" and "movsb" are 1 byte each, "movsw" is two bytes.
	 * (In 16-bit mode, "movsl" is 2 bytes and "movsw" is 1 byte,
	 * but "movsl" moves twice as much data, so it balances out).
	 *
	 * The cutoff point therefore occurs around 26 bytes; the byte
	 * requirements for each method are:
	 *
	 * len		   16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31
	 * #bytes (ecx)	    8  8  8  8  8  8  8  8  8  8  8  8  8  8  8  8
	 * #bytes (no ecx)  4  5  6  7  5  6  7  8  6  7  8  9  7  8  9 10
	 */

	esi = src;
	edi = dest;
	
	if ( len >= 26 )
		return __memcpy ( dest, src, len );
	
	if ( len >= 6*4 )
		__asm__ __volatile__ ( "movsl" : "=&D" ( edi ), "=&S" ( esi )
				       : "0" ( edi ), "1" ( esi ) : "memory" );
	if ( len >= 5*4 )
		__asm__ __volatile__ ( "movsl" : "=&D" ( edi ), "=&S" ( esi )
				       : "0" ( edi ), "1" ( esi ) : "memory" );
	if ( len >= 4*4 )
		__asm__ __volatile__ ( "movsl" : "=&D" ( edi ), "=&S" ( esi )
				       : "0" ( edi ), "1" ( esi ) : "memory" );
	if ( len >= 3*4 )
		__asm__ __volatile__ ( "movsl" : "=&D" ( edi ), "=&S" ( esi )
				       : "0" ( edi ), "1" ( esi ) : "memory" );
	if ( len >= 2*4 )
		__asm__ __volatile__ ( "movsl" : "=&D" ( edi ), "=&S" ( esi )
				       : "0" ( edi ), "1" ( esi ) : "memory" );
	if ( len >= 1*4 )
		__asm__ __volatile__ ( "movsl" : "=&D" ( edi ), "=&S" ( esi )
				       : "0" ( edi ), "1" ( esi ) : "memory" );
	if ( ( len % 4 ) >= 2 )
		__asm__ __volatile__ ( "movsw" : "=&D" ( edi ), "=&S" ( esi )
				       : "0" ( edi ), "1" ( esi ) : "memory" );
	if ( ( len % 2 ) >= 1 )
		__asm__ __volatile__ ( "movsb" : "=&D" ( edi ), "=&S" ( esi )
				       : "0" ( edi ), "1" ( esi ) : "memory" );

	return dest;
}

/**
 * Copy memory area
 *
 * @v dest		Destination address
 * @v src		Source address
 * @v len		Length
 * @ret dest		Destination address
 */
static inline __attribute__ (( always_inline )) void *
memcpy ( void *dest, const void *src, size_t len ) {
	if ( __builtin_constant_p ( len ) ) {
		return __constant_memcpy ( dest, src, len );
	} else {
		return __memcpy ( dest, src, len );
	}
}

#define __HAVE_ARCH_MEMMOVE

extern void * __memmove ( void *dest, const void *src, size_t len );

/**
 * Copy (possibly overlapping) memory area
 *
 * @v dest		Destination address
 * @v src		Source address
 * @v len		Length
 * @ret dest		Destination address
 */
static inline __attribute__ (( always_inline )) void *
memmove ( void *dest, const void *src, size_t len ) {
	ssize_t offset = ( dest - src );

	if ( __builtin_constant_p ( offset ) ) {
		if ( offset <= 0 ) {
			return memcpy ( dest, src, len );
		} else {
			return __memcpy_reverse ( dest, src, len );
		}
	} else {
		return __memmove ( dest, src, len );
	}
}

#define __HAVE_ARCH_MEMSET

/**
 * Fill memory region
 *
 * @v dest		Destination address
 * @v fill		Fill pattern
 * @v len		Length
 * @ret dest		Destination address
 */
static inline void * memset ( void *dest, int fill, size_t len ) {
	void *discard_D;
	size_t discard_c;

	__asm__ __volatile__ ( "rep stosb"
			       : "=&D" ( discard_D ), "=&c" ( discard_c )
			       : "0" ( dest ), "1" ( len ), "a" ( fill )
			       : "memory" );
	return dest;
}

#define __HAVE_ARCH_MEMSWAP
static inline void * memswap(void *dest, void *src, size_t n)
{
long d0, d1, d2, d3;
__asm__ __volatile__(
	"\n1:\t"
	"movb (%2),%%al\n\t"
	"xchgb (%1),%%al\n\t"
	"inc %1\n\t"
	"stosb\n\t"
	"loop 1b"
	: "=&c" (d0), "=&S" (d1), "=&D" (d2), "=&a" (d3)
	: "0" (n), "1" (src), "2" (dest)
	: "memory" );
return dest;
}

#define __HAVE_ARCH_STRNCMP

extern int strncmp ( const char *str1, const char *str2, size_t len );

#define __HAVE_ARCH_STRLEN

extern size_t strlen ( const char *string );

#endif /* ETHERBOOT_BITS_STRING_H */
