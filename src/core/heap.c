#include "etherboot.h"
#include "init.h"
#include "memsizes.h"
#include "heap.h"

struct heap_block {
	size_t size;
	char data[0];
};

size_t heap_ptr, heap_top, heap_bot;

#define _virt_start 0

static void init_heap(void)
{
	size_t size;
	size_t start, end;
	unsigned i;
	/* Find the largest contiguous area of memory that
	 * I can use for the heap, which is organized as 
	 * a stack that grows backwards through memory.
	 */

	/* If I have virtual address that do not equal physical addresses
	 * there is a change I will try to use memory from both sides of
	 * the virtual address space simultaneously, which can cause all kinds
	 * of interesting problems.
	 * Avoid it by logically extending etherboot.  Once I know that relocation
	 * works I can just start the virtual address space at 0, and this problem goes
	 * away so that is probably a better solution.
	 */
#if 0
	start = virt_to_phys(_text);
#else
	/* segment wrap around is nasty don't chance it. */
	start = virt_to_phys(_virt_start);
#endif
	end  = virt_to_phys(_end);
	size = 0;
	for(i = 0; i < meminfo.map_count; i++) {
		unsigned long r_start, r_end;
		if (meminfo.map[i].type != E820_RAM)
			continue;
		if (meminfo.map[i].addr > ULONG_MAX)
			continue;
		if (meminfo.map[i].size > ULONG_MAX)
			continue;
		
		r_start = meminfo.map[i].addr;
		r_end = r_start + meminfo.map[i].size;
		if (r_end < r_start) {
			r_end = ULONG_MAX;
		}
		/* Handle areas that overlap etherboot */
		if ((end > r_start) && (start < r_end)) {
			/* Etherboot completely covers the region */
			if ((start <= r_start) && (end >= r_end))
				continue;
			/* Etherboot is completely contained in the region */
			if ((start > r_start) && (end < r_end)) {
				/* keep the larger piece */
				if ((r_end - end) >= (r_start - start)) {
					r_start = end;
				}
				else {
					r_end = start;
				}
			}
			/* Etherboot covers one end of the region.
			 * Shrink the region.
			 */
			else if (end >= r_end) {
				r_end = start;
			}
			else if (start <= r_start) {
				r_start = end;
			}
		}
		/* If two areas are the size prefer the greater address */
		if (((r_end - r_start) > size) ||
			(((r_end - r_start) == size) && (r_start > heap_top))) {
			size = r_end - r_start;
			heap_top = r_start;
			heap_bot = r_end;
		}
	}
	if (size == 0) {
		printf("init_heap: No heap found.\n");
		exit(1);
	}
	heap_ptr = heap_bot;
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
	if ( addr < heap_top ) {
		return NULL;
	}

	block = phys_to_virt ( addr );
	block->size = ( heap_ptr - addr );
	heap_ptr = addr;
	return block->data;
}

/*
 * Allocate all remaining space on the heap
 *
 */
void * emalloc_all ( size_t *size ) {
	*size = heap_ptr - heap_top - sizeof ( struct heap_block );
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

	ASSERT ( heap_ptr <= heap_bot );
}

/*
 * Free all allocated heap blocks
 *
 */
void efree_all ( void ) {
	heap_ptr = heap_bot;
}

INIT_FN ( INIT_HEAP, init_heap, efree_all, NULL );
