/*
 *  Copyright (C) 2004 Tobias Lorenz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef ETHERBOOT_BITS_ENDIAN_H
#define ETHERBOOT_BITS_ENDIAN_H

#ifdef __ARMEB__
#define __BYTE_ORDER __BIG_ENDIAN
#else
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif

#endif /* ETHERBOOT_BITS_ENDIAN_H */
