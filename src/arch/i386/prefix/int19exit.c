#include "bochs.h"
#include "realmode.h"

/**
 * The "exit via INT 19" exit path.
 *
 * INT 19 is the old (pre-BBS) "boot system" interrupt.  It is
 * conventionally used now to return from a failed boot from floppy
 * disk.
 *
 * @bug Not yet implemented
 *
 */
void exit_via_int19 ( struct i386_all_regs *ix86 ) {
	bochsbp();
	/* Placeholder */
}
