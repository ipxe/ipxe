#ifdef PCBIOS

#include "etherboot.h"
#include "realmode.h" /* for real_mode_stack */

/* Routines to allocate base memory in a BIOS-compatible way, by
 * updating the Free Base Memory Size counter at 40:13h.
 *
 * Michael Brown <mbrown@fensystems.co.uk> (mcb30)
 * $Id$
 */

#define fbms ( ( uint16_t * ) phys_to_virt ( 0x413 ) )
#define BASE_MEMORY_MAX ( 640 )
#define FREE_BLOCK_MAGIC ( ('!'<<0) + ('F'<<8) + ('R'<<16) + ('E'<<24) )
#define FREE_BASE_MEMORY ( (uint32_t) ( *fbms << 10 ) )

/* Prototypes */
void * _allot_base_memory ( size_t size );
void _forget_base_memory ( void *ptr, size_t size );

typedef struct free_base_memory_block {
	uint32_t	magic;
	uint16_t	size_kb;
} free_base_memory_block_t;

/* Return amount of free base memory in bytes
 */

uint32_t get_free_base_memory ( void ) {
	return FREE_BASE_MEMORY;
}

/* Start of our image in base memory.
 */
#define __text16_nocompress __attribute__ ((section (".text16.nocompress")))
uint32_t image_basemem __text16_nocompress = 0;
uint32_t image_basemem_size __text16_nocompress = 0;

/* Allot/free the real-mode stack
 */

void allot_real_mode_stack ( void )
{
	void *new_real_mode_stack;

	if ( lock_real_mode_stack ) 
		return;

	/* This is evil hack. 
	 * Until we have a real_mode stack use 0x7c00.
	 * Except for 0 - 0x600 membory below 0x7c00 is hardly every used.
	 * This stack should never be used unless the stack allocation fails,
	 * or if someone has placed a print statement in a dangerous location.
	 */
	if (!real_mode_stack) {
		real_mode_stack = 0x7c00;
	}
	new_real_mode_stack = _allot_base_memory ( real_mode_stack_size );
	if ( ! new_real_mode_stack ) {
		printf ( "FATAL: No real-mode stack\n" );
		while ( 1 ) {};
	}
	real_mode_stack = virt_to_phys ( new_real_mode_stack );
	get_memsizes();
}

void forget_real_mode_stack ( void )
{
	if ( lock_real_mode_stack ) 
		return;

	if ( real_mode_stack) {
		_forget_base_memory ( phys_to_virt(real_mode_stack),
				      real_mode_stack_size );
		/* get_memsizes() uses the real_mode stack we just freed
		 * for it's BIOS calls.
		 */
		get_memsizes();
		real_mode_stack = 0;
	}
}

/* Allocate N bytes of base memory.  Amount allocated will be rounded
 * up to the nearest kB, since that's the granularity of the BIOS FBMS
 * counter.  Returns NULL if memory cannot be allocated.
 */

static void * _allot_base_memory ( size_t size ) 
{
	uint16_t size_kb = ( size + 1023 ) >> 10;
	void *ptr = NULL;

#ifdef DEBUG_BASEMEM
	printf ( "Trying to allocate %d kB of base memory from %d kB free\n",
		 size_kb, *fbms );
#endif

	/* Free up any unused memory before we start */
	free_unused_base_memory();

	/* Check available base memory */
	if ( size_kb > *fbms ) { return NULL; }

	/* Reduce available base memory */
	*fbms -= size_kb;

	/* Calculate address of memory allocated */
	ptr = phys_to_virt ( FREE_BASE_MEMORY );

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

#ifdef DEBUG_BASEMEM
	printf ( "Allocated %d kB at [%x,%x)\n", size_kb,
		 virt_to_phys ( ptr ),
		 virt_to_phys ( ptr ) + size_kb * 1024 );
#endif

	return ptr;
}

void * allot_base_memory ( size_t size )
{
	void *ptr;

	/* Free real-mode stack, allocate memory, reallocate real-mode
	 * stack.
	 */
	forget_real_mode_stack();
	ptr = _allot_base_memory ( size );
	get_memsizes();
	return ptr;
}

/* Free base memory allocated by allot_base_memory.  The BIOS provides
 * nothing better than a LIFO mechanism for freeing memory (i.e. it
 * just has the single "total free memory" counter), but we improve
 * upon this slightly; as long as you free all the allotted blocks, it
 * doesn't matter what order you free them in.  (This will only work
 * for blocks that are freed via forget_base_memory()).
 *
 * Yes, it's annoying that you have to remember the size of the blocks
 * you've allotted.  However, since our granularity of allocation is
 * 1K, the alternative is to risk wasting the occasional kB of base
 * memory, which is a Bad Thing.  Really, you should be using as
 * little base memory as possible, so consider the awkwardness of the
 * API to be a feature! :-)
 */

static void _forget_base_memory ( void *ptr, size_t size ) 
{
	uint16_t remainder = virt_to_phys(ptr) & 1023;
	uint16_t size_kb = ( size + remainder + 1023 ) >> 10;
	free_base_memory_block_t *free_block =
		( free_base_memory_block_t * ) ( ptr - remainder );
	
	if ( ( ptr == NULL ) || ( size == 0 ) ) { 
		return; 
	}

#ifdef DEBUG_BASEMEM
	printf ( "Trying to free %d bytes base memory at 0x%x\n",
		 size, virt_to_phys ( ptr ) );
	if ( remainder > 0 ) {
		printf ( "WARNING: destructively expanding free block "
			 "downwards to 0x%x\n",
			 virt_to_phys ( ptr - remainder ) );
	}
#endif

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
	while ( size_kb > 0 ) {
		/* Mark this block as unused */
		free_block->magic = FREE_BLOCK_MAGIC;
		free_block->size_kb = size_kb;
		/* Move up by 1 kB */
		free_block = (void *)(((char *)free_block) + (1 << 10));
		size_kb--;
	}

	/* Free up unused base memory */
	free_unused_base_memory();
}

void forget_base_memory ( void *ptr, size_t size )
{
	/* Free memory, free real-mode stack, re-allocate real-mode
	 * stack.  Do this so that we don't end up wasting a huge
	 * block of memory trapped behind the real-mode stack.
	 */
	_forget_base_memory ( ptr, size );
	forget_real_mode_stack();
	get_memsizes();
}

/* Do the actual freeing of memory.  This is split out from
 * forget_base_memory() so that it may be called separately.  It
 * should be called whenever base memory is deallocated by an external
 * entity (if we can detect that it has done so) so that we get the
 * chance to free up our own blocks.
 */
static void free_unused_base_memory ( void ) {
	free_base_memory_block_t *free_block = NULL;

	/* Try to release memory back to the BIOS.  Free all
	 * consecutive blocks marked as free.
	 */
	while ( 1 ) {
		/* Calculate address of next potential free block */
		free_block = ( free_base_memory_block_t * )
			phys_to_virt ( FREE_BASE_MEMORY );
		
		/* Stop processing if we're all the way up to 640K or
		 * if this is not a free block
		 */
		if ( ( *fbms == BASE_MEMORY_MAX ) ||
		     ( free_block->magic != FREE_BLOCK_MAGIC ) ) {
			break;
		}

		/* Return memory to BIOS */
		*fbms += free_block->size_kb;

#ifdef DEBUG_BASEMEM
		printf ( "Freed %d kB base memory, %d kB now free\n",
			 free_block->size_kb, *fbms );
#endif
		
		/* Zero out freed block.  We do this in case
		 * the block contained any structures that
		 * might be located by scanning through
		 * memory.
		 */
		memset ( free_block, 0, free_block->size_kb << 10 );
	}
}

/* Free base memory used by the prefix.  Called once at start of
 * Etherboot by arch_main().
 */
void forget_prefix_base_memory ( void )
{
	/* runtime_start_kb is _text rounded down to a physical kB boundary */
	uint32_t runtime_start_kb = virt_to_phys(_text) & ~0x3ff;
	/* prefix_size_kb is the prefix size excluding any portion
	 * that overlaps into the first kB used by the runtime image
	 */
	uint32_t prefix_size_kb = runtime_start_kb - image_basemem;

#ifdef DEBUG_BASEMEM
	printf ( "Attempting to free base memory used by prefix\n" );
#endif

	/* If the decompressor is in allocated base memory
	 * *and* the Etherboot text is in base
	 * memory, then free the decompressor.
	 */
	if ( ( image_basemem >= FREE_BASE_MEMORY ) &&
	     ( runtime_start_kb >= FREE_BASE_MEMORY ) &&
	     ( runtime_start_kb <= ( BASE_MEMORY_MAX << 10 ) ) ) 
	{
		forget_base_memory ( phys_to_virt ( image_basemem ),
				     prefix_size_kb );
		/* Update image_basemem and image_basemem_size to
		 * indicate that our allocation now starts with _text
		 */
		image_basemem = runtime_start_kb;
		image_basemem_size -= prefix_size_kb;
	}
}

/* Free base memory used by the runtime image.  Called after
 * relocation by arch_relocated_from().
 */
void forget_runtime_base_memory ( unsigned long old_addr )
{
	/* text_start_kb is old _text rounded down to a physical KB boundary */
	uint32_t old_text_start_kb = old_addr & ~0x3ff;

#ifdef DEBUG_BASEMEM
	printf ( "Attempting to free base memory used by runtime image\n" );
#endif

	if ( ( image_basemem >= FREE_BASE_MEMORY ) &&
	     ( image_basemem == old_text_start_kb ) ) 
	{
		forget_base_memory ( phys_to_virt ( image_basemem ),
				     image_basemem_size );
		/* Update image_basemem to show no longer in use */
		image_basemem = 0;
		image_basemem_size = 0;
	}
}

#endif /* PCBIOS */
