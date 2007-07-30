#ifndef _LIBGCC_H
#define _LIBGCC_H

#include <stdint.h>
#include <stddef.h>

/*
 * It seems as though gcc expects its implicit arithmetic functions to
 * be cdecl, even if -mrtd is specified.  This is somewhat
 * inconsistent; for example, if -mregparm=3 is used then the implicit
 * functions do become regparm(3).
 *
 * The implicit calls to memcpy() and memset() which gcc can generate
 * do not seem to have this inconsistency; -mregparm and -mrtd affect
 * them in the same way as any other function.
 *
 */
#define LIBGCC __attribute__ (( cdecl ))

extern LIBGCC uint64_t __udivmoddi4(uint64_t num, uint64_t den, uint64_t *rem);
extern LIBGCC uint64_t __udivdi3(uint64_t num, uint64_t den);
extern LIBGCC uint64_t __umoddi3(uint64_t num, uint64_t den);
extern LIBGCC int64_t __divdi3(int64_t num, int64_t den);
extern LIBGCC int64_t __moddi3(int64_t num, int64_t den);

#endif /* _LIBGCC_H */
