/*
 *  Copyright (C) 2004 Tobias Lorenz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __LIMITS_H
#define __LIMITS_H  1

#define MB_LEN_MAX	16

#define CHAR_BIT	8

#define SCHAR_MIN	(-128)
#define SCHAR_MAX	127

#define UCHAR_MAX	255

#define CHAR_MIN	SCHAR_MIN
#define CHAR_MAX	SCHAR_MAX

#define SHRT_MIN	(-32768)
#define SHRT_MAX	32767

#define USHRT_MAX	65535

#define INT_MIN	(-INT_MAX - 1)
#define INT_MAX	2147483647

#define UINT_MAX	4294967295U

#define LONG_MAX	2147483647L
#define LONG_MIN	(-LONG_MAX - 1L)

#define ULONG_MAX	4294967295UL

#define LLONG_MAX	9223372036854775807LL
#define LLONG_MIN	(-LLONG_MAX - 1LL)

#define ULLONG_MAX	18446744073709551615ULL

#endif
