#include "stdint.h"
#include "stddef.h"
#include "registers.h"
#include "string.h"
#include "hooks.h"
#include "init.h"
#include "main.h"
#ifdef REALMODE
#include "realmode.h"
#endif

/* Symbols defined by the linker */
extern char _bss[], _ebss[];

/*
 * This file provides the basic entry points from assembly code.  See
 * README.i386 for a description of the entry code path.
 *
 * This file is compiled to two different object files: hooks.o and
 * hooks_rm.o.  REALMODE is defined when compiling hooks_rm.o
 *
 */

/*
 * arch_initialise(): perform any required initialisation such as
 * setting up the console device and relocating to high memory.  Note
 * that if we relocate to high memory and the prefix is in base
 * memory, then we will need to install a copy of librm in base memory
 * and adjust retaddr so that we return to the installed copy.
 *
 */
#ifdef REALMODE
void arch_rm_initialise ( struct i386_all_regs *regs,
			  void (*retaddr) (void) )
#else /* REALMODE */
void arch_initialise ( struct i386_all_regs *regs,
		       void (*retaddr) (void) __unused )
#endif /* REALMODE */
{
	/* Zero the BSS */
	memset ( _bss, 0, _ebss - _bss );

	/* Call all registered initialisation functions.
	 */
	call_init_fns ();
}

#ifdef REALMODE

/*
 * arch_rm_main() : call main() and then exit via whatever exit mechanism
 * the prefix requested.
 *
 */
void arch_rm_main ( struct i386_all_regs *regs ) {
	struct i386_all_regs regs_copy;
	void (*exit_fn) ( struct i386_all_regs *regs );

	/* Take a copy of the registers, because the memory holding
	 * them will probably be trashed by the time main() returns.
	 */
	regs_copy = *regs;
	exit_fn = ( typeof ( exit_fn ) ) regs_copy.eax;

	/* Call to main() */
	regs_copy.eax = main();

	/* Call registered per-object exit functions */
	call_exit_fns ();

	if ( exit_fn ) {
		/* Prefix requested that we use a particular function
		 * as the exit path, so we call this function, which
		 * must not return.
		 */
		exit_fn ( &regs_copy );
	}
}

#else /* REALMODE */

/*
 * arch_main() : call main() and return
 *
 */
void arch_main ( struct i386_all_regs *regs ) {
	regs->eax = main();

	/* Call registered per-object exit functions */
	call_exit_fns ();
};

#endif /* REALMODE */
