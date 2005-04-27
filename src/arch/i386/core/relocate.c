#include "virtaddr.h"
#include "memsizes.h"
#include "osdep.h"
#include "etherboot.h"
#include "init.h"
#include "relocate.h"

#ifndef KEEP_IT_REAL

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

/*
 * relocate() must be called without any hardware resources pointing
 * at the current copy of Etherboot.  The easiest way to achieve this
 * is to call relocate() from within arch_initialise(), before the NIC
 * gets touched in any way.
 *
 */

/*
 * The linker passes in the symbol _max_align, which is the alignment
 * that we must preserve, in bytes.
 *
 */
extern char _max_align[];
#define max_align ( ( unsigned int ) _max_align )

/* Linker symbols */
extern char _text[];
extern char _end[];

/* Post-relocation function table */
static struct post_reloc_fn post_reloc_fns[0] __table_start(post_reloc_fn);
static struct post_reloc_fn post_reloc_fns_end[0] __table_end(post_reloc_fn);

static void relocate ( void ) {
	unsigned long addr, eaddr, size;
	unsigned i;
	struct post_reloc_fn *post_reloc_fn;

	/* Walk through the memory map and find the highest address
	 * below 4GB that etherboot will fit into.  Ensure etherboot
	 * lies entirely within a range with A20=0.  This means that
	 * even if something screws up the state of the A20 line, the
	 * etherboot code is still visible and we have a chance to
	 * diagnose the problem.
	 */

	/* First find the size of etherboot, including enough space to
	 * pad it to the required alignment
	 */
	size = _end - _text + max_align - 1;

	/* Current end address of Etherboot.  If the current etherboot
	 * is beyond MAX_ADDR pretend it is at the lowest possible
	 * address.
	 */
	eaddr = virt_to_phys(_end);
	if ( eaddr > MAX_ADDR ) {
		eaddr = 0;
	}

	DBG ( "Relocate: currently at [%x,%x)\n"
	      "...need %x bytes for %d-byte alignment\n",
	      virt_to_phys ( _text ), eaddr, size, max_align );

	for ( i = 0; i < meminfo.map_count; i++ ) {
		unsigned long r_start, r_end;

		DBG ( "Considering [%x%x,%x%x)\n",
		      ( unsigned long ) ( meminfo.map[i].addr >> 32 ),
		      ( unsigned long ) meminfo.map[i].addr,
		      ( unsigned long )
		       ( ( meminfo.map[i].addr + meminfo.map[i].size ) >> 32 ),
		      ( unsigned long )
		       ( meminfo.map[i].addr + meminfo.map[i].size ) );
		
		/* Check block is usable memory */
		if (meminfo.map[i].type != E820_RAM) {
			DBG ( "...not RAM\n" );
			continue;
		}

		/* Truncate block to MAX_ADDR.  This will be less than
		 * 4GB, which means that we can get away with using
		 * just 32-bit arithmetic after this stage.
		 */
		if ( meminfo.map[i].addr > MAX_ADDR ) {
			DBG ( "...starts after MAX_ADDR=%x\n", MAX_ADDR );
			continue;
		}
		r_start = meminfo.map[i].addr;
		if ( meminfo.map[i].addr + meminfo.map[i].size > MAX_ADDR ) {
			r_end = MAX_ADDR;
			DBG ( "...end truncated to MAX_ADDR=%x\n", MAX_ADDR );
		} else {
			r_end = meminfo.map[i].addr + meminfo.map[i].size;
		}

		/* Shrink the range down to use only even megabytes
		 * (i.e. A20=0).
		 */
		if ( ( r_end - 1 ) & 0x100000 ) {
			/* If last byte that might be used (r_end-1)
			 * is in an odd megabyte, round down r_end to
			 * the top of the next even megabyte.
			 */
			r_end = ( r_end - 1 ) & ~0xfffff;
			DBG ( "...end truncated to %x "
			      "(avoid ending in odd megabyte)\n",
			      r_end );
		} else if ( ( r_end - size ) & 0x100000 ) {
			/* If the last byte that might be used
			 * (r_end-1) is in an even megabyte, but the
			 * first byte that might be used (r_end-size)
			 * is an odd megabyte, round down to the top
			 * of the next even megabyte.
			 * 
			 * Make sure that we don't accidentally wrap
			 * r_end below 0.
			 */
			if ( r_end > 0x100000 ) {
				r_end = ( r_end - 0x100000 ) & ~0xfffff;
				DBG ( "...end truncated to %x "
				      "(avoid starting in odd megabyte)\n",
				      r_end );
			}
		}

		DBG ( "...usable portion is [%x,%x)\n", r_start, r_end );

		/* If we have rounded down r_end below r_ start, skip
		 * this block.
		 */
		if ( r_end < r_start ) {
			DBG ( "...truncated to negative size\n" );
			continue;
		}

		/* Check that there is enough space to fit in Etherboot */
		if ( r_end - r_start < size ) {
			DBG ( "...too small (need %x bytes)\n", size );
			continue;
		}

		/* If the start address of the Etherboot we would
		 * place in this block is higher than the end address
		 * of the current highest block, use this block.
		 *
		 * Note that this avoids overlaps with the current
		 * Etherboot, as well as choosing the highest of all
		 * viable blocks.
		 */
		if ( r_end - size > eaddr ) {
			eaddr = r_end;
			DBG ( "...new best block found.\n" );
		}
	}

	DBG ( "New location will be in [%x,%x)\n", eaddr - size, eaddr );

	/* Calculate new location of Etherboot, and align it to the
	 * required alignemnt.
	 */
	addr = eaddr - size;
	addr += ( virt_to_phys ( _text ) - addr ) & ( max_align - 1 );
	DBG ( "After alignment, new location is [%x,%x)\n",
	      addr, addr + _end - _text );

	if ( addr != virt_to_phys ( _text ) ) {
		DBG ( "Relocating _text from: [%lx,%lx) to [%lx,%lx)\n",
		      virt_to_phys ( _text ), virt_to_phys ( _end ),
		      addr, addr + _end - _text );

		relocate_to ( addr );
		/* Note that we cannot make real-mode calls
		 * (e.g. printf) at this point, because librm has just
		 * been moved to high memory.
		 */

		/* Call any registered post-relocation functions.
		 * librm has a post-relocation function to install a
		 * new librm into base memory.
		 */
		for ( post_reloc_fn = post_reloc_fns;
		      post_reloc_fn < post_reloc_fns_end ; post_reloc_fn++ ) {
			if ( post_reloc_fn->post_reloc )
				post_reloc_fn->post_reloc ();
		}
		
	}
}

INIT_FN ( INIT_RELOCATE, relocate, NULL, NULL );

#endif /* ! KEEP_IT_REAL */
