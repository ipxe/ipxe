#include "etherboot.h"
#include "init.h"
#include "memsizes.h"
#include "heap.h"

struct heap_block {
	size_t size;
	char data[0];
};

/* Linker symbols */
extern char _text[];
extern char _end[];

static unsigned long heap_start, heap_end, heap_ptr;

/*
 * Find the largest contiguous area of memory that I can use for the
 * heap.
 *
 */
static void init_heap ( void ) {
	unsigned int i;
	unsigned long eb_start, eb_end;
	unsigned long size;

	size = 0;
	
	/* Region occupied by Etherboot */
	eb_start = virt_to_phys ( _text );
	eb_end = virt_to_phys ( _end );

	for ( i = 0 ; i < meminfo.map_count ; i++ ) {
		unsigned long r_start, r_end, r_size;
		unsigned long pre_eb, post_eb;

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

	ASSERT ( size != 0 );
	DBG ( "HEAP using region [%x,%x)\n", heap_start, heap_end );
	heap_ptr = heap_end;
}

/*
 * Allocate a block from the heap.
 *
 */
void * emalloc ( size_t size, unsigned int align ) {
	physaddr_t addr;
	struct heap_block *block;
	
	ASSERT ( ( align & ( align - 1 ) ) == 0 );
	
	addr = ( ( ( heap_ptr - size ) & ~( align - 1 ) )
		 - sizeof ( struct heap_block ) );
	if ( addr < heap_start ) {
		DBG ( "HEAP no space for %x bytes (alignment %d) in [%x,%x)\n",
		      size, align, heap_start, heap_ptr );
		return NULL;
	}

	block = phys_to_virt ( addr );
	block->size = ( heap_ptr - addr );
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
void efree ( void *ptr ) {
	struct heap_block *block;

	ASSERT ( ptr == phys_to_virt ( heap_ptr + sizeof ( size_t ) ) );
	
	block = ( struct heap_block * )
		( ptr - offsetof ( struct heap_block, data ) );
	heap_ptr += block->size;

	DBG ( "HEAP freed %x [%x,%x)\n", virt_to_phys ( ptr ),
	      virt_to_phys ( block ), heap_ptr );

	ASSERT ( heap_ptr <= heap_end );
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

INIT_FN ( INIT_HEAP, init_heap, efree_all, NULL );
