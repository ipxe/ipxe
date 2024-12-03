#ifndef _BITS_SETJMP_H
#define _BITS_SETJMP_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** A jump buffer */
typedef struct {
	uint64_t s0;
	uint64_t s1;
	uint64_t s2;
	uint64_t s3;
	uint64_t s4;
	uint64_t s5;
	uint64_t s6;
	uint64_t s7;
	uint64_t s8;

	uint64_t fp;
	uint64_t sp;
	uint64_t ra;
} jmp_buf[1];

#endif /* _BITS_SETJMP_H */
