#ifndef ETHERBOOT_BITS_BYTESWAP_H
#define ETHERBOOT_BITS_BYTESWAP_H

static inline __attribute__ ((always_inline)) uint16_t
__i386_bswap_16(uint16_t x)
{
	__asm__("xchgb %b0,%h0\n\t"
		: "=q" (x)
		: "0" (x));
	return x;
}

static inline __attribute__ ((always_inline)) uint32_t
__i386_bswap_32(uint32_t x)
{
	__asm__("xchgb %b0,%h0\n\t"
		"rorl $16,%0\n\t"
		"xchgb %b0,%h0"
		: "=q" (x)
		: "0" (x));
	return x;
}

static inline __attribute__ ((always_inline)) uint64_t
__i386_bswap_64(uint64_t x)
{
	union {
		uint64_t qword;
		uint32_t dword[2]; 
	} u;

	u.qword = x;
	u.dword[0] = __i386_bswap_32(u.dword[0]);
	u.dword[1] = __i386_bswap_32(u.dword[1]);
	__asm__("xchgl %0,%1"
		: "=r" ( u.dword[0] ), "=r" ( u.dword[1] )
		: "0" ( u.dword[0] ), "1" ( u.dword[1] ) );
	return u.qword;
}

#define __bswap_constant_16(x) \
	((uint16_t)((((uint16_t)(x) & 0x00ff) << 8) | \
		    (((uint16_t)(x) & 0xff00) >> 8)))

#define __bswap_constant_32(x) \
	((uint32_t)((((uint32_t)(x) & 0x000000ffU) << 24) | \
		    (((uint32_t)(x) & 0x0000ff00U) <<  8) | \
		    (((uint32_t)(x) & 0x00ff0000U) >>  8) | \
		    (((uint32_t)(x) & 0xff000000U) >> 24)))

#define __bswap_constant_64(x) \
	((uint64_t)((((uint64_t)(x) & 0x00000000000000ffULL) << 56) | \
		    (((uint64_t)(x) & 0x000000000000ff00ULL) << 40) | \
		    (((uint64_t)(x) & 0x0000000000ff0000ULL) << 24) | \
		    (((uint64_t)(x) & 0x00000000ff000000ULL) <<  8) | \
		    (((uint64_t)(x) & 0x000000ff00000000ULL) >>  8) | \
		    (((uint64_t)(x) & 0x0000ff0000000000ULL) >> 24) | \
		    (((uint64_t)(x) & 0x00ff000000000000ULL) >> 40) | \
		    (((uint64_t)(x) & 0xff00000000000000ULL) >> 56)))

#define __bswap_16(x) \
	((uint16_t)(__builtin_constant_p(x) ? \
	__bswap_constant_16(x) : \
	__i386_bswap_16(x)))

#define __bswap_32(x) \
	((uint32_t)(__builtin_constant_p(x) ? \
	__bswap_constant_32(x) : \
	__i386_bswap_32(x)))

#define __bswap_64(x) \
	((uint64_t)(__builtin_constant_p(x) ? \
	__bswap_constant_64(x) : \
	__i386_bswap_64(x)))

#endif /* ETHERBOOT_BITS_BYTESWAP_H */
