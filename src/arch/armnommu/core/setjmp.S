/*
 *  Copyright (C) 2004 Tobias Lorenz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
.text

.global sigsetjmp;
.type sigsetjmp,%function
.align 4;
sigsetjmp:
	/* Save registers */
	stmia	r0, {v1-v6, sl, fp, sp, lr}

	mov	r0, #0
	bx	lr
.size sigsetjmp,.-sigsetjmp;



.global longjmp;
.type longjmp,%function
.align 4;
longjmp:
	mov	ip, r0		/* save jmp_buf pointer */

	movs	r0, r1		/* get the return value in place */
	moveq	r0, #1		/* can't let setjmp() return zero! */

	ldmia	ip, {v1-v6, sl, fp, sp, pc}
.size longjmp,.-longjmp;
