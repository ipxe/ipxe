#ifndef _BITS_VIRT_OFFSET_H
#define _BITS_VIRT_OFFSET_H

/** @file
 *
 * RISCV-specific virtual address offset
 *
 * We use the thread pointer register (tp) to hold the virtual address
 * offset, so that virtual-to-physical address translations work as
 * expected even while we are executing directly from read-only memory
 * (and so cannot store a value in a global virt_offset variable).
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/**
 * Read virtual address offset held in thread pointer register
 *
 * @ret virt_offset	Virtual address offset
 */
static inline __attribute__ (( const, always_inline )) unsigned long
tp_virt_offset ( void ) {
	register unsigned long tp asm ( "tp" );

	__asm__ ( "" : "=r" ( tp ) );
	return tp;
}

/** Always read thread pointer register to get virtual address offset */
#define virt_offset tp_virt_offset()

#endif /* _BITS_VIRT_OFFSET_H */
