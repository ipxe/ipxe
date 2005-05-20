#include "bochs.h"
#include "realmode.h"

/** @file
 *
 * The "exit via INT 19" exit path.
 *
 * INT 19 is the old (pre-BBS) "boot system" interrupt.  It is
 * conventionally used now to return from a failed boot from floppy
 * disk.
 *
 */

/**
 * Exit via INT19
 *
 * @v ix86		i386 register values to be loaded on exit
 * @ret Never
 * @err None
 *
 * Exit back to the BIOS by switching to real mode, reloading the
 * registers as they were before Etherboot started, and executing INT
 * 19.
 *
 * @bug Not yet implemented
 *
 */
void exit_via_int19 ( struct i386_all_regs *ix86 ) {
	bochsbp();
	/* Placeholder */
}
