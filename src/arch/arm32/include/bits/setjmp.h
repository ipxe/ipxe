#ifndef _BITS_SETJMP_H
#define _BITS_SETJMP_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

/** A jump buffer */
typedef struct {
	/** Saved r4 */
	uint32_t r4;
	/** Saved r5 */
	uint32_t r5;
	/** Saved r6 */
	uint32_t r6;
	/** Saved r7 */
	uint32_t r7;
	/** Saved r8 */
	uint32_t r8;
	/** Saved r9 */
	uint32_t r9;
	/** Saved r10 */
	uint32_t r10;
	/** Saved frame pointer (r11) */
	uint32_t fp;
	/** Saved stack pointer (r13) */
	uint32_t sp;
	/** Saved link register (r14) */
	uint32_t lr;
} jmp_buf[1];

#endif /* _BITS_SETJMP_H */
