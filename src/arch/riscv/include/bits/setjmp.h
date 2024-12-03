#ifndef _BITS_SETJMP_H
#define _BITS_SETJMP_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** A jump buffer */
typedef struct {
	/** Return address (ra) */
	unsigned long ra;
	/** Stack pointer (sp) */
	unsigned long sp;
	/** Callee-saved registers (s0-s11) */
	unsigned long s[12];
} jmp_buf[1];

#endif /* _BITS_SETJMP_H */
