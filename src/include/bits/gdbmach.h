#ifndef _BITS_GDBMACH_H
#define _BITS_GDBMACH_H

/** @file
 *
 * Dummy GDB architecture specifics
 *
 * This file is included only if the architecture does not provide its
 * own version of this file.
 *
 */

#include <stdint.h>

typedef unsigned long gdbreg_t;

/* Register snapshot */
enum {
	/* Not yet implemented */
	GDBMACH_NREGS,
};

#define GDBMACH_SIZEOF_REGS ( GDBMACH_NREGS * sizeof ( gdbreg_t ) )

static inline void gdbmach_set_pc ( gdbreg_t *regs, gdbreg_t pc ) {
	/* Not yet implemented */
	( void ) regs;
	( void ) pc;
}

static inline void gdbmach_set_single_step ( gdbreg_t *regs, int step ) {
	/* Not yet implemented */
	( void ) regs;
	( void ) step;
}

static inline void gdbmach_breakpoint ( void ) {
	/* Not yet implemented */
}

extern int gdbmach_set_breakpoint ( int type, unsigned long addr, size_t len,
				    int enable );
extern void gdbmach_init ( void );

#endif /* _BITS_GDBMACH_H */
