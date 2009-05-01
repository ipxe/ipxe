#include <gpxe/io.h>
#include <registers.h>
#include <gpxe/memmap.h>

/*
 * Originally by Eric Biederman
 *
 * Heavily modified by Michael Brown 
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

/*
 * The linker passes in the symbol _max_align, which is the alignment
 * that we must preserve, in bytes.
 *
 */
extern char _max_align[];
#define max_align ( ( unsigned int ) _max_align )

/* Linker symbols */
extern char _textdata[];
extern char _etextdata[];

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
 * This finds a suitable location for Etherboot near the top of 32-bit
 * address space, and returns the physical address of the new location
 * to the prefix in %edi.
 */
__asmcall void relocate ( struct i386_all_regs *ix86 ) {
	struct memory_map memmap;
	unsigned long start, end, size, padded_size;
	unsigned long new_start, new_end;
	unsigned i;

	/* Get memory map and current location */
	get_memmap ( &memmap );
	start = virt_to_phys ( _textdata );
	end = virt_to_phys ( _etextdata );
	size = ( end - start );
	padded_size = ( size + max_align - 1 );

	DBG ( "Relocate: currently at [%lx,%lx)\n"
	      "...need %lx bytes for %d-byte alignment\n",
	      start, end, padded_size, max_align );

	/* Walk through the memory map and find the highest address
	 * below 4GB that etherboot will fit into.  Ensure etherboot
	 * lies entirely within a range with A20=0.  This means that
	 * even if something screws up the state of the A20 line, the
	 * etherboot code is still visible and we have a chance to
	 * diagnose the problem.
	 */
	new_end = end;
	for ( i = 0 ; i < memmap.count ; i++ ) {
		struct memory_region *region = &memmap.regions[i];
		unsigned long r_start, r_end;

		DBG ( "Considering [%llx,%llx)\n", region->start, region->end);
		
		/* Truncate block to MAX_ADDR.  This will be less than
		 * 4GB, which means that we can get away with using
		 * just 32-bit arithmetic after this stage.
		 */
		if ( region->start > MAX_ADDR ) {
			DBG ( "...starts after MAX_ADDR=%lx\n", MAX_ADDR );
			continue;
		}
		r_start = region->start;
		if ( region->end > MAX_ADDR ) {
			DBG ( "...end truncated to MAX_ADDR=%lx\n", MAX_ADDR );
			r_end = MAX_ADDR;
		} else {
			r_end = region->end;
		}
		
		/* Shrink the range down to use only even megabytes
		 * (i.e. A20=0).
		 */
		if ( ( r_end - 1 ) & 0x100000 ) {
			/* If last byte that might be used (r_end-1)
			 * is in an odd megabyte, round down r_end to
			 * the top of the next even megabyte.
			 *
			 * Make sure that we don't accidentally wrap
			 * r_end below 0.
			 */
			if ( r_end >= 1 ) {
				r_end = ( r_end - 1 ) & ~0xfffff;
				DBG ( "...end truncated to %lx "
				      "(avoid ending in odd megabyte)\n",
				      r_end );
			}
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
			if ( r_end >= 0x100000 ) {
				r_end = ( r_end - 0x100000 ) & ~0xfffff;
				DBG ( "...end truncated to %lx "
				      "(avoid starting in odd megabyte)\n",
				      r_end );
			}
		}

		DBG ( "...usable portion is [%lx,%lx)\n", r_start, r_end );

		/* If we have rounded down r_end below r_ start, skip
		 * this block.
		 */
		if ( r_end < r_start ) {
			DBG ( "...truncated to negative size\n" );
			continue;
		}

		/* Check that there is enough space to fit in Etherboot */
		if ( ( r_end - r_start ) < size ) {
			DBG ( "...too small (need %lx bytes)\n", size );
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
		if ( ( r_end - size ) > new_end ) {
			new_end = r_end;
			DBG ( "...new best block found.\n" );
		}
	}

	/* Calculate new location of Etherboot, and align it to the
	 * required alignemnt.
	 */
	new_start = new_end - padded_size;
	new_start += ( start - new_start ) & ( max_align - 1 );
	new_end = new_start + size;

	DBG ( "Relocating from [%lx,%lx) to [%lx,%lx)\n",
	      start, end, new_start, new_end );
	
	/* Let prefix know what to copy */
	ix86->regs.esi = start;
	ix86->regs.edi = new_start;
	ix86->regs.ecx = size;
}
