/*
 *  Copyright (C) 2004 Tobias Lorenz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
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

# define __bswap_16(x) \
    (__extension__							      \
     ({ unsigned short int __bsx = (x);					      \
        ((((__bsx) >> 8) & 0xff) | (((__bsx) & 0xff) << 8)); }))


# define __bswap_32(x) \
    (__extension__							      \
     ({ unsigned int __bsx = (x);					      \
        ((((__bsx) & 0xff000000) >> 24) | (((__bsx) & 0x00ff0000) >>  8) |    \
	 (((__bsx) & 0x0000ff00) <<  8) | (((__bsx) & 0x000000ff) << 24)); }))

#endif /* ETHERBOOT_BITS_BYTESWAP_H */
