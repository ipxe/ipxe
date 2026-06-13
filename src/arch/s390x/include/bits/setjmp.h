#ifndef _BITS_SETJMP_H
#define _BITS_SETJMP_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** A jump buffer */
typedef struct {
	/** Callee-saved registers (r6-r15) */
	unsigned long regs[12];
} jmp_buf[1];

#endif /* _BITS_SETJMP_H */
