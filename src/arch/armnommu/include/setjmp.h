/*
 *  Copyright (C) 2004 Tobias Lorenz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef ETHERBOOT_SETJMP_H
#define ETHERBOOT_SETJMP_H

#ifndef __ASSEMBLER__
/* Jump buffer contains v1-v6, sl, fp, sp and pc.  Other registers are not
   saved.  */
//typedef int jmp_buf[22];
typedef int jmp_buf[10];
#endif

extern int sigsetjmp(jmp_buf __env, int __savemask);
extern void longjmp(jmp_buf __env, int __val) __attribute__((__noreturn__));

#define setjmp(env) sigsetjmp(env,0)

#endif /* ETHERBOOT_SETJMP_H */
