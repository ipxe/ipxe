#include "bochs.h"
#include "realmode.h"

/*
 * The "exit via INT 19" exit path.  INT 19 is the old (pre-BBS) "boot
 * system" interrupt.
 *
 */

void exit_via_int19 ( struct real_mode_regs *rm_regs ) {
	bochsbp();
	/* Placeholder */
}
