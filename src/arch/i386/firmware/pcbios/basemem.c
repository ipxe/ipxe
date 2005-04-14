#include "stdint.h"
#include "stddef.h"
#include "memsizes.h"
#include "etherboot.h"
#include "basemem.h"

/* Routines to allocate base memory in a BIOS-compatible way, by
 * updating the Free Base Memory Size counter at 40:13h.
 *
 * Michael Brown <mbrown@fensystems.co.uk> (mcb30)
 *
 * We no longer have anything to do with the real-mode stack.  The
 * only code that can end up creating a huge bubble of wasted base
 * memory is the UNDI driver, so we make it the responsibility of the
 * UNDI driver to reallocate the real-mode stack if required.
 */

/* "fbms" is an alias to the BIOS FBMS counter at 40:13, and acts just
 * like any other uint16_t.  We can't be used under -DKEEP_IT_REAL
 * anyway, so we may as well be efficient.
 */
#define fbms ( * ( ( uint16_t * ) phys_to_virt ( 0x413 ) ) )
#define FBMS_MAX ( 640 )

/* Local prototypes */
static void free_unused_base_memory ( void );

/*
 * Return amount of free base memory in bytes
 *
 */
uint32_t get_free_base_memory ( void ) {
	return fbms << 10;
}

/* Allocate N bytes of base memory.  Amount allocated will be rounded
 * up to the nearest kB, since that's the granularity of the BIOS FBMS
 * counter.  Returns NULL if memory cannot be allocated.
 *
 */
void * alloc_base_memory ( size_t size ) {
	uint16_t size_kb = ( size + 1023 ) >> 10;
	void *ptr;

	DBG ( "Trying to allocate %d bytes of base memory from %d kB free\n",
	      size, fbms );

	/* Free up any unused memory before we start */
	free_unused_base_memory();

	/* Check available base memory */
	if ( size_kb > fbms ) {
		DBG ( "Could not allocate %d kB of base memory: "
		      "only %d kB free\n", size_kb, fbms );
		return NULL;
	}

	/* Reduce available base memory */
	fbms -= size_kb;

	/* Calculate address of memory allocated */
	ptr = phys_to_virt ( fbms << 10 );

	/* Zero out memory.  We do this so that allocation of
	 * already-used space will show up in the form of a crash as
	 * soon as possible.
	 *
	 * Update: there's another reason for doing this.  If we don't
	 * zero the contents, then they could still retain our "free
	 * block" markers and be liable to being freed whenever a
	 * base-memory allocation routine is next called.
	 */
	memset ( ptr, 0, size_kb << 10 );

	DBG ( "Allocated %d kB of base memory at [%hx:0000,%hx:0000), "
	      "%d kB now free\n", size_kb,
	      ( virt_to_phys ( ptr ) >> 4 ),
	      ( ( virt_to_phys ( ptr ) + ( size_kb << 10 ) ) >> 4 ), fbms );

	/* Update our memory map */
	get_memsizes();

	return ptr;
}

/* Free base memory allocated by alloc_base_memory.  The BIOS provides
 * nothing better than a LIFO mechanism for freeing memory (i.e. it
 * just has the single "total free memory" counter), but we improve
 * upon this slightly; as long as you free all the allocated blocks, it
 * doesn't matter what order you free them in.  (This will only work
 * for blocks that are freed via free_base_memory()).
 *
 * Yes, it's annoying that you have to remember the size of the blocks
 * you've allocated.  However, since our granularity of allocation is
 * 1K, the alternative is to risk wasting the occasional kB of base
 * memory, which is a Bad Thing.  Really, you should be using as
 * little base memory as possible, so consider the awkwardness of the
 * API to be a feature! :-)
 *
 */
void free_base_memory ( void *ptr, size_t size ) {
	uint16_t remainder = virt_to_phys ( ptr ) & 1023;
	uint16_t size_kb = ( size + remainder + 1023 ) >> 10;
	union free_base_memory_block *free_block = 
		( ( void * ) ( ptr - remainder ) );
	
	if ( ( ptr == NULL ) || ( size == 0 ) ) { 
		return; 
	}

	DBG ( "Trying to free %d bytes base memory at %hx:%hx "
	      "from %d kB free\n", size,
	      ( virt_to_phys ( ptr - remainder ) >> 4 ),
	      ( virt_to_phys ( ptr - remainder ) & 0xf ) + remainder,
	      fbms );

	/* Mark every kilobyte within this block as free.  This is
	 * overkill for normal purposes, but helps when something has
	 * allocated base memory with a granularity finer than the
	 * BIOS granularity of 1kB.  PXE ROMs tend to do this when
	 * they allocate their own memory.  This method allows us to
	 * free their blocks (admittedly in a rather dangerous,
	 * tread-on-anything-either-side sort of way, but there's no
	 * other way to do it).
	 *
	 * Since we're marking every kB as free, there's actually no
	 * need for recording the size of the blocks.  However, we
	 * keep this in so that debug messages are friendlier.  It
	 * probably adds around 8 bytes to the overall code size.
	 */
	for ( ; size_kb > 0 ; free_block++, size_kb-- ) {
		/* Mark this block as unused */
		free_block->magic = FREE_BLOCK_MAGIC;
		free_block->size_kb = size_kb;
	}

	/* Free up unused base memory */
	free_unused_base_memory();

	/* Update our memory map */
	get_memsizes();
}

/* Do the actual freeing of memory.  This is split out from
 * free_base_memory() so that it may be called separately.  It
 * should be called whenever base memory is deallocated by an external
 * entity (if we can detect that it has done so) so that we get the
 * chance to free up our own blocks.
 */
static void free_unused_base_memory ( void ) {
	union free_base_memory_block *free_block;

	/* Try to release memory back to the BIOS.  Free all
	 * consecutive blocks marked as free.
	 */
	while ( 1 ) {
		/* Calculate address of next potential free block */
		free_block = phys_to_virt ( fbms << 10 );
		
		/* Stop processing if we're all the way up to 640K or
		 * if this is not a free block
		 */
		if ( ( fbms == FBMS_MAX ) ||
		     ( free_block->magic != FREE_BLOCK_MAGIC ) ) {
			break;
		}

		/* Return memory to BIOS */
		fbms += free_block->size_kb;

		DBG ( "Freed %d kB of base memory at [%hx:0000,%hx:0000), "
		      "%d kB now free\n",
		      free_block->size_kb,
		      ( virt_to_phys ( free_block ) >> 4 ),
		      ( ( virt_to_phys ( free_block ) + 
			  ( free_block->size_kb << 10 ) ) >> 4 ),
		      fbms );
		
		/* Do not zero out the freed block, because it might
		 * be the one containing librm, in which case we're
		 * going to have severe problems the next time we use
		 * DBG() or, failing that, call get_memsizes().
		 */
	}
}
