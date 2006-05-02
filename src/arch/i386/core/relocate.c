#include <virtaddr.h>
#include <registers.h>
#include <memsizes.h>

/*
 * Originally by Eric Biederman
 *
 * Heavily modified by Michael Brown 
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

/* within 1MB of 4GB is too close. 
 * MAX_ADDR is the maximum address we can easily do DMA to.
 *
 * Not sure where this constraint comes from, but kept it from Eric's
 * old code - mcb30
 */
#define MAX_ADDR (0xfff00000UL)

/**
 * Relocate Etherboot
 *
 * @v ix86		x86 register dump from prefix
 * @ret ix86		x86 registers to return to prefix
 *
 * This copies Etherboot to a suitable location near the top of 32-bit
 * address space, and returns the physical address of the new location
 * to the prefix in %edi.
 */
void relocate ( struct i386_all_regs *ix86 ) {
	unsigned long addr, eaddr, size;
	unsigned i;

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

		memcpy ( phys_to_virt ( addr ), _text, _end - _text );
	}
	
	/* Let prefix know where the new copy is */
	ix86->regs.edi = addr;
}
