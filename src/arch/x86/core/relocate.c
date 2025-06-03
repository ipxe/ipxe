#include <ipxe/uaccess.h>
#include <ipxe/memmap.h>
#include <registers.h>

/*
 * Originally by Eric Biederman
 *
 * Heavily modified by Michael Brown 
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

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

/* Preserve alignment to a 4kB page
 *
 * Required for x86_64, and doesn't hurt for i386.
 */
#define ALIGN 4096

/**
 * Relocate iPXE
 *
 * @v ebp		Maximum address to use for relocation
 * @ret esi		Current physical address
 * @ret edi		New physical address
 * @ret ecx		Length to copy
 *
 * This finds a suitable location for iPXE near the top of 32-bit
 * address space, and returns the physical address of the new location
 * to the prefix in %edi.
 */
__asmcall void relocate ( struct i386_all_regs *ix86 ) {
	struct memmap_region region;
	physaddr_t start, end, max;
	physaddr_t new_start, new_end;
	physaddr_t r_start, r_end;
	size_t size, padded_size;

	/* Show whole memory map (for debugging) */
	memmap_dump_all ( 0 );

	/* Get current location */
	start = virt_to_phys ( _textdata );
	end = virt_to_phys ( _etextdata );
	size = ( end - start );
	padded_size = ( size + ALIGN - 1 );

	DBGC ( &region, "Relocate: currently at [%#08lx,%#08lx)\n"
	       "...need %#zx bytes for %d-byte alignment\n",
	       start, end, padded_size, ALIGN );

	/* Determine maximum usable address */
	max = MAX_ADDR;
	if ( ix86->regs.ebp < max ) {
		max = ix86->regs.ebp;
		DBGC ( &region, "Limiting relocation to [0,%#08lx)\n", max );
	}

	/* Walk through the memory map and find the highest address
	 * above the current iPXE and below 4GB that iPXE will fit
	 * into.
	 */
	new_end = end;
	for_each_memmap_from ( &region, end, 0 ) {

		/* Truncate block to maximum address.  This will be
		 * strictly less than 4GB, which means that we can get
		 * away with using just 32-bit arithmetic after this
		 * stage.
		 */
		DBGC_MEMMAP ( &region, &region );
		if ( region.min > max ) {
			DBGC ( &region, "...starts after max=%#08lx\n", max );
			break;
		}
		r_start = region.min;
		if ( ! memmap_is_usable ( &region ) ) {
			DBGC ( &region, "...not usable\n" );
			continue;
		}
		r_end = ( r_start + memmap_size ( &region ) );
		if ( ( r_end == 0 ) || ( r_end > max ) ) {
			DBGC ( &region, "...end truncated to max=%#08lx\n",
			       max );
			r_end = max;
		}
		DBGC ( &region, "...usable portion is [%#08lx,%#08lx)\n",
		       r_start, r_end );

		/* Check that there is enough space to fit in iPXE */
		if ( ( r_end - r_start ) < padded_size ) {
			DBGC ( &region, "...too small (need %#zx bytes)\n",
			       padded_size );
			continue;
		}

		/* Use highest block with enough space */
		new_end = r_end;
		DBGC ( &region, "...new best block found.\n" );
	}

	/* Calculate new location of iPXE, and align it to the
	 * required alignemnt.
	 */
	new_start = new_end - padded_size;
	new_start += ( ( start - new_start ) & ( ALIGN - 1 ) );
	new_end = new_start + size;

	DBGC ( &region, "Relocating from [%#08lx,%#08lx) to [%#08lx,%#08lx)\n",
	       start, end, new_start, new_end );

	/* Let prefix know what to copy */
	ix86->regs.esi = start;
	ix86->regs.edi = new_start;
	ix86->regs.ecx = size;
}
