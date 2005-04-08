#ifndef STDDEF_H
#define STDDEF_H

/* for size_t */
#include "stdint.h"

#undef NULL
#define NULL	((void *)0)

#undef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#endif /* STDDEF_H */
