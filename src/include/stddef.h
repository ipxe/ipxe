#ifndef STDDEF_H
#define STDDEF_H

FILE_LICENCE ( GPL2_ONLY );

/* for size_t */
#include <stdint.h>

#undef NULL
#define NULL ((void *)0)

#undef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#undef container_of
#define container_of(ptr, type, member) ({                      \
	const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
	(type *)( (char *)__mptr - offsetof(type,member) );})

/* __WCHAR_TYPE__ is defined by gcc and will change if -fshort-wchar is used */
#ifndef __WCHAR_TYPE__
#define __WCHAR_TYPE__ long int
#endif
typedef __WCHAR_TYPE__ wchar_t;

#endif /* STDDEF_H */
