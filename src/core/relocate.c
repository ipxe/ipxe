#ifndef	NORELOCATE

#include "etherboot.h"

/* by Eric Biederman */

/* On some platforms etherboot is compiled as a shared library, and we use
 * the ELF pic support to make it relocateable.  This works very nicely
 * for code, but since no one has implemented PIC data yet pointer
 * values in variables are a a problem.  Global variables are a
 * pain but the return addresses on the stack are the worst.  On these
 * platforms relocate_to will restart etherboot, to ensure the stack
 * is reinitialize and hopefully get the global variables
 * appropriately reinitialized as well.
 * 
 */

void relocate(void)
{
	unsigned long addr, eaddr, size;
	unsigned i;
	/* Walk through the memory map and find the highest address
	 * below 4GB that etherboot will fit into.  Ensure etherboot
	 * lies entirely within a range with A20=0.  This means that
	 * even if something screws up the state of the A20 line, the
	 * etherboot code is still visible and we have a chance to
	 * diagnose the problem.
	 */
	/* First find the size of etherboot */
	addr = virt_to_phys(_text);
	eaddr = virt_to_phys(_end);
	size = (eaddr - addr + 0xf) & ~0xf;

	/* If the current etherboot is beyond MAX_ADDR pretend it is
	 * at the lowest possible address.
	 */
	if (eaddr > MAX_ADDR) {
		eaddr = 0;
	}

	for(i = 0; i < meminfo.map_count; i++) {
		unsigned long r_start, r_end;
		if (meminfo.map[i].type != E820_RAM) {
			continue;
		}
		if (meminfo.map[i].addr > MAX_ADDR) {
			continue;
		}
		if (meminfo.map[i].size > MAX_ADDR) {
			continue;
		}
		r_start = meminfo.map[i].addr;
		r_end = r_start + meminfo.map[i].size;
		/* Make the addresses 16 byte (128 bit) aligned */
		r_start = (r_start + 15) & ~15;
		r_end = r_end & ~15;
		if (r_end < r_start) {
			r_end = MAX_ADDR;
		}
		if (r_end < size) {
			/* Avoid overflow weirdness when r_end - size < 0 */
			continue;
		}
		/* Shrink the range down to use only even megabytes
		 * (i.e. A20=0).
		 */
		if ( r_end & 0x100000 ) {
			/* If r_end is in an odd megabyte, round down
			 * r_end to the top of the next even megabyte.
			 */
			r_end = r_end & ~0xfffff;
		} else if ( ( r_end - size ) & 0x100000 ) {
			/* If r_end is in an even megabyte, but the
			 * start of Etherboot would be in an odd
			 * megabyte, round down to the top of the next
			 * even megabyte.
			*/
			r_end = ( r_end - 0x100000 ) & ~0xfffff;
		}
		/* If we have rounded down r_end below r_ start, skip
		 * this block.
		 */
		if ( r_end < r_start ) {
			continue;
		}
		if (eaddr < r_end - size) {
			addr = r_end - size;
			eaddr = r_end;
		}
	}
	if (addr != virt_to_phys(_text)) {
		unsigned long old_addr = virt_to_phys(_text);
		printf("Relocating _text from: [%lx,%lx) to [%lx,%lx)\n",
			old_addr, virt_to_phys(_end),
			addr, eaddr);
		arch_relocate_to(addr);
		cleanup();
		relocate_to(addr);
		arch_relocated_from(old_addr);
	}
}

#endif /* NORELOCATE */
