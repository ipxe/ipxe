#ifndef ETHERBOOT_BITS_BYTESWAP_H
#define ETHERBOOT_BITS_BYTESWAP_H

static inline uint64_t __ia64_bswap_64(uint64_t x)
{
	uint64_t result;
	__asm__ volatile(
		"mux1 %0=%1,@rev" : 
		"=r" (result) 
		: "r" (x));
	return result;
}

#define __bswap_constant_16(x) \
	((uint16_t)((((uint16_t)(x) & 0x00ff) << 8) | \
		(((uint16_t)(x) & 0xff00) >> 8)))

#define __bswap_constant_32(x) \
	((uint32_t)((((uint32_t)(x) & 0x000000ffU) << 24) | \
		(((uint32_t)(x) & 0x0000ff00U) <<  8) | \
		(((uint32_t)(x) & 0x00ff0000U) >>  8) | \
		(((uint32_t)(x) & 0xff000000U) >> 24)))

#define __bswap_16(x) \
	(__builtin_constant_p(x) ? \
	__bswap_constant_16(x) : \
	(__ia64_bswap_64(x) >> 48))


#define __bswap_32(x) \
	(__builtin_constant_p(x) ? \
	__bswap_constant_32(x) : \
	(__ia64_bswap_64(x) >> 32))


#endif /* ETHERBOOT_BITS_BYTESWAP_H */
