#ifndef _BITS_LKRN_H
#define _BITS_LKRN_H

/** @file
 *
 * Linux kernel image invocation
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** Header magic value */
#define LKRN_MAGIC_ARCH LKRN_MAGIC_AARCH64

/**
 * Jump to kernel entry point
 *
 * @v entry		Kernel entry point
 * @v fdt		Device tree
 */
static inline __attribute__ (( noreturn )) void
lkrn_jump ( physaddr_t entry, physaddr_t fdt ) {
	register unsigned long x0 asm ( "x0" ) = fdt;

	__asm__ __volatile__ ( "br %1"
			       : : "r" ( x0 ), "r" ( entry ) );
	__builtin_unreachable();
}

#endif /* _BITS_LKRN_H */
