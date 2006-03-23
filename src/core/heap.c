#include "etherboot.h"
#include "init.h"
#include "memsizes.h"
#include <assert.h>
#include "heap.h"

struct heap_block {
	size_t size;
	unsigned int align;
	char data[0];
};

/* Linker symbols */
extern char _text[];
extern char _end[];

static physaddr_t heap_start;

/*
 * Find the largest contiguous area of memory that I can use for the
 * heap.
 *
 */
static void init_heap ( void ) {
	unsigned int i;
	physaddr_t eb_start, eb_end;
	physaddr_t size;

	size = 0;
	
	/* Region occupied by Etherboot */
	eb_start = virt_to_phys ( _text );
	eb_end = virt_to_phys ( _end );

	for ( i = 0 ; i < meminfo.map_count ; i++ ) {
		physaddr_t r_start, r_end, r_size;
		physaddr_t pre_eb, post_eb;

		/* Get start and end addresses of the region */
		if ( meminfo.map[i].type != E820_RAM )
			continue;
		if ( meminfo.map[i].addr > ULONG_MAX )
			continue;
		r_start = meminfo.map[i].addr;
		if ( r_start + meminfo.map[i].size > ULONG_MAX ) {
			r_end = ULONG_MAX;
		} else {
			r_end = r_start + meminfo.map[i].size;
		}
		
		/* Avoid overlap with Etherboot.  When Etherboot is
		 * completely contained within the region, choose the
		 * larger of the two remaining portions.
		 */
		if ( ( eb_start < r_end ) && ( eb_end > r_start ) ) {
			pre_eb = ( eb_start > r_start ) ?
				( eb_start - r_start ) : 0;
			post_eb = ( r_end > eb_end ) ?
				( r_end - eb_end ) : 0;
			if ( pre_eb > post_eb ) {
				r_end = eb_start;
			} else {
				r_start = eb_end;
			}
		}

		/* Use the biggest region.  Where two regions are the
		 * same size, use the later region.  (Provided that
		 * the memory map is laid out in a sensible order,
		 * this should give us the higher region.)
		 */
		r_size = r_end - r_start;
		if ( r_size >= size ) {
			heap_start = r_start;
			heap_end = r_end;
			size = r_size;
		}
	}

	assert ( size != 0 );
	DBG ( "HEAP using region [%x,%x)\n", heap_start, heap_end );
	heap_ptr = heap_end;
}

/*
 * Allocate a block from the heap.
 *
 */
static inline physaddr_t block_alloc_addr ( physaddr_t heap_ptr,
					    size_t size, unsigned int align ) {
	return ( ( ( heap_ptr - size ) & ~( align - 1 ) )
		 - sizeof ( struct heap_block ) );
}

void * emalloc ( size_t size, unsigned int align ) {
	struct heap_block *block;
	physaddr_t addr;
	
	assert ( ( align & ( align - 1 ) ) == 0 );

	addr = block_alloc_addr ( heap_ptr, size, align );
	if ( addr < heap_start ) {
		DBG ( "HEAP no space for %x bytes (alignment %d) in [%x,%x)\n",
		      size, align, heap_start, heap_ptr );
		return NULL;
	}

	block = phys_to_virt ( addr );
	block->size = ( heap_ptr - addr );
	block->align = align;
	DBG ( "HEAP allocated %x bytes (alignment %d) at %x [%x,%x)\n",
	      size, align, virt_to_phys ( block->data ), addr, heap_ptr );
	heap_ptr = addr;
	return block->data;
}

/*
 * Allocate all remaining space on the heap
 *
 */
void * emalloc_all ( size_t *size ) {
	*size = heap_ptr - heap_start - sizeof ( struct heap_block );
	return emalloc ( *size, sizeof ( void * ) );
}

/*
 * Free a heap block
 *
 */
static inline physaddr_t block_free_addr ( size_t size ) {
	return heap_ptr + size;
}

void efree ( void *ptr ) {
	struct heap_block *block;

	assert ( ptr == phys_to_virt ( heap_ptr + sizeof ( size_t ) ) );
	
	block = ( struct heap_block * )
		( ptr - offsetof ( struct heap_block, data ) );
	heap_ptr = block_free_addr ( block->size );

	DBG ( "HEAP freed %x [%x,%x)\n", virt_to_phys ( ptr ),
	      virt_to_phys ( block ), heap_ptr );

	assert ( heap_ptr <= heap_end );
}

/*
 * Free all allocated heap blocks
 *
 */
void efree_all ( void ) {
	DBG ( "HEAP discarding allocated blocks in [%x,%x)\n",
	      heap_ptr, heap_end );

	heap_ptr = heap_end;
}

/*
 * Resize a heap block
 *
 */
void * erealloc ( void *ptr, size_t size ) {
	struct heap_block *old_block;
	size_t old_size;
	unsigned int old_align;
	physaddr_t new_addr;
	size_t move_size;
	
	/* Get descriptor of the old block */
	old_block = ( struct heap_block * )
		( ptr - offsetof ( struct heap_block, data ) );
	old_size = old_block->size;
	old_align = old_block->align;

	/* Check that allocation is going to succeed */
	new_addr = block_alloc_addr ( block_free_addr ( old_size ),
				      size, old_align );
	if ( new_addr < heap_start ) {
		DBG ( "HEAP no space for %x bytes (alignment %d) in [%x,%x)\n",
		      size, align, heap_start, block_free_addr ( old_size ) );
		return NULL;
	}

	/* Free the old block */
	efree ( ptr );

	/* Move the data.  Do this *before* allocating the new block,
	 * because the new block's descriptor may overwrite the old
	 * block's data, if the new block is smaller than the old
	 * block.
	 */
	move_size = size + sizeof ( struct heap_block );
	if ( old_size < move_size )
		move_size = old_size;
	memmove ( phys_to_virt ( new_addr ), old_block, move_size );

	/* Allocate the new block.  This must succeed, because we
	 * already checked that there was sufficient space.
	 */
	ptr = emalloc ( size, old_align );
	assert ( ptr != NULL );

	return ptr;
}

INIT_FN ( INIT_HEAP, init_heap, efree_all, NULL );
