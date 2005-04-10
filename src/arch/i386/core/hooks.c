#include "stdint.h"
#include "stddef.h"
#include "registers.h"
#include "string.h"
#include "hooks.h"
#include "init.h"
#include "main.h"
#include "relocate.h"
#include "etherboot.h"

/* Symbols defined by the linker */
extern char _bss[], _ebss[];

/*
 * This file provides the basic entry points from assembly code.  See
 * README.i386 for a description of the entry code path.
 *
 */

/*
 * arch_initialise(): perform any required initialisation such as
 * setting up the console device and relocating to high memory.  Note
 * that if we relocate to high memory and the prefix is in base
 * memory, then we will need to install a copy of librm in base
 * memory.  librm's reset function takes care of this.
 *
 */

#include "librm.h"

void arch_initialise ( struct i386_all_regs *regs __unused ) {
	/* Zero the BSS */
	memset ( _bss, 0, _ebss - _bss );

	/* Call all registered initialisation functions.
	 */
	call_init_fns ();

	/* Relocate to high memory.  (This is a no-op under
	 * -DKEEP_IT_REAL.)
	 */
	relocate();

	/* Call all registered reset functions.  Note that if librm is
	 * included, it is the reset function that will install a
	 * fresh copy of librm in base memory.  It follows from this
	 * that (a) librm must be first in the reset list and (b) you
	 * cannot call console output functions between relocate() and
	 * call_reset_fns(), because real-mode calls will crash the
	 * machine.
	 */
	call_reset_fns();

	printf ( "init finished\n" );

	regs->es = virt_to_phys ( installed_librm ) >> 4;

	__asm__ ( "xchgw %bx, %bx" );
}

/*
 * arch_main() : call main() and then exit via whatever exit mechanism
 * the prefix requested.
 *
 */
void arch_main ( struct i386_all_regs *regs ) {
	void (*exit_path) ( struct i386_all_regs *regs );

	/* Determine exit path requested by prefix */
	exit_path = ( typeof ( exit_path ) ) regs->eax;

	/* Call to main() */
	regs->eax = main();

	/* Call registered per-object exit functions */
	call_exit_fns ();

	if ( exit_path ) {
		/* Prefix requested that we use a particular function
		 * as the exit path, so we call this function, which
		 * must not return.
		 */
		exit_path ( regs );
	}
}
