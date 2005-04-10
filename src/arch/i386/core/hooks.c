#include "registers.h"
#include "main.h"
#include "etherboot.h"
#include "hooks.h"

/* Symbols defined by the linker */
extern char _bss[], _ebss[];

/*
 * This file provides the basic entry points from assembly code.  See
 * README.i386 for a description of the entry code path.
 *
 */

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

	if ( exit_path ) {
		/* Prefix requested that we use a particular function
		 * as the exit path, so we call this function, which
		 * must not return.
		 */
		exit_path ( regs );
	}
}
