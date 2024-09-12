#ifndef _SETJMP_H
#define _SETJMP_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <bits/setjmp.h>

extern int __asmcall __attribute__ (( returns_twice ))
setjmp ( jmp_buf env );

extern void __asmcall __attribute__ (( noreturn ))
longjmp ( jmp_buf env, int val );

#endif /* _SETJMP_H */
