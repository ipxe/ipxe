#ifndef ETHERBOOT_BITS_BYTESWAP_H
#define ETHERBOOT_BITS_BYTESWAP_H

/* We do not have byte swap functions ... We are
 * RISC processor ...
 */

static inline unsigned short __swap16(volatile unsigned short v)
{
    return ((v << 8) | (v >> 8));
}

static inline unsigned int __swap32(volatile unsigned long v)
{
    return ((v << 24) | ((v & 0xff00) << 8) | ((v & 0xff0000) >> 8) | (v >> 24));
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
	__swap16(x))


#define __bswap_32(x) \
	(__builtin_constant_p(x) ? \
	__bswap_constant_32(x) : \
	__swap32(x))

#endif /* ETHERBOOT_BITS_BYTESWAP_H */
