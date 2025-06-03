#ifndef _BITS_LKRN_H
#define _BITS_LKRN_H

/** @file
 *
 * Linux kernel image invocation
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/hart.h>

/** Header magic value */
#define LKRN_MAGIC_ARCH LKRN_MAGIC_RISCV

/**
 * Jump to kernel entry point
 *
 * @v entry		Kernel entry point
 * @v fdt		Device tree
 */
static inline __attribute__ (( noreturn )) void
lkrn_jump ( physaddr_t entry, physaddr_t fdt ) {
	register unsigned long a0 asm ( "a0" ) = boot_hart;
	register unsigned long a1 asm ( "a1" ) = fdt;

	__asm__ __volatile__ ( "call disable_paging\n\t"
			       "jr %2\n\t"
			       : : "r" ( a0 ), "r" ( a1 ), "r" ( entry ) );
	__builtin_unreachable();
}

#endif /* _BITS_LKRN_H */
