/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ipxe/io.h>
#include <ipxe/list.h>
#include <ipxe/init.h>
#include <ipxe/refcnt.h>
#include <ipxe/malloc.h>
#include <valgrind/memcheck.h>

/** @file
 *
 * Dynamic memory allocation
 *
 */

/** A free block of memory */
struct memory_block {
	/** Size of this block */
	size_t size;
	/** Padding
	 *
	 * This padding exists to cover the "count" field of a
	 * reference counter, in the common case where a reference
	 * counter is the first element of a dynamically-allocated
	 * object.  It avoids clobbering the "count" field as soon as
	 * the memory is freed, and so allows for the possibility of
	 * detecting reference counting errors.
	 */
	char pad[ offsetof ( struct refcnt, count ) +
		  sizeof ( ( ( struct refcnt * ) NULL )->count ) ];
	/** List of free blocks */
	struct list_head list;
};

/** Physical address alignment maintained for free blocks of memory
 *
 * We keep memory blocks aligned on a power of two that is at least
 * large enough to hold a @c struct @c memory_block.
 */
#define MIN_MEMBLOCK_ALIGN ( 4 * sizeof ( void * ) )

/** A block of allocated memory complete with size information */
struct autosized_block {
	/** Size of this block */
	size_t size;
	/** Remaining data */
	char data[0];
};

/**
 * Heap area size
 *
 * Currently fixed at 512kB.
 */
#define HEAP_SIZE ( 512 * 1024 )

/** Heap area alignment */
#define HEAP_ALIGN MIN_MEMBLOCK_ALIGN

/** The heap area */
static char __attribute__ (( aligned ( HEAP_ALIGN ) )) heap_area[HEAP_SIZE];

/**
 * Mark all blocks in free list as defined
 *
 * @v heap		Heap
 */
static inline void valgrind_make_blocks_defined ( struct heap *heap ) {
	struct memory_block *block;

	/* Do nothing unless running under Valgrind */
	if ( RUNNING_ON_VALGRIND <= 0 )
		return;

	/* Traverse free block list, marking each block structure as
	 * defined.  Some contortions are necessary to avoid errors
	 * from list_check().
	 */

	/* Mark block list itself as defined */
	VALGRIND_MAKE_MEM_DEFINED ( &heap->blocks, sizeof ( heap->blocks ) );

	/* Mark areas accessed by list_check() as defined */
	VALGRIND_MAKE_MEM_DEFINED ( &heap->blocks.prev->next,
				    sizeof ( heap->blocks.prev->next ) );
	VALGRIND_MAKE_MEM_DEFINED ( heap->blocks.next,
				    sizeof ( *heap->blocks.next ) );
	VALGRIND_MAKE_MEM_DEFINED ( &heap->blocks.next->next->prev,
				    sizeof ( heap->blocks.next->next->prev ) );

	/* Mark each block in list as defined */
	list_for_each_entry ( block, &heap->blocks, list ) {

		/* Mark block as defined */
		VALGRIND_MAKE_MEM_DEFINED ( block, sizeof ( *block ) );

		/* Mark areas accessed by list_check() as defined */
		VALGRIND_MAKE_MEM_DEFINED ( block->list.next,
					    sizeof ( *block->list.next ) );
		VALGRIND_MAKE_MEM_DEFINED ( &block->list.next->next->prev,
				      sizeof ( block->list.next->next->prev ) );
	}
}

/**
 * Mark all blocks in free list as inaccessible
 *
 * @v heap		Heap
 */
static inline void valgrind_make_blocks_noaccess ( struct heap *heap ) {
	struct memory_block *block;
	struct memory_block *prev = NULL;

	/* Do nothing unless running under Valgrind */
	if ( RUNNING_ON_VALGRIND <= 0 )
		return;

	/* Traverse free block list, marking each block structure as
	 * inaccessible.  Some contortions are necessary to avoid
	 * errors from list_check().
	 */

	/* Mark each block in list as inaccessible */
	list_for_each_entry ( block, &heap->blocks, list ) {

		/* Mark previous block (if any) as inaccessible. (Current
		 * block will be accessed by list_check().)
		 */
		if ( prev )
			VALGRIND_MAKE_MEM_NOACCESS ( prev, sizeof ( *prev ) );
		prev = block;

		/* At the end of the list, list_check() will end up
		 * accessing the first list item.  Temporarily mark
		 * this area as defined.
		 */
		VALGRIND_MAKE_MEM_DEFINED ( &heap->blocks.next->prev,
					    sizeof ( heap->blocks.next->prev ));
	}
	/* Mark last block (if any) as inaccessible */
	if ( prev )
		VALGRIND_MAKE_MEM_NOACCESS ( prev, sizeof ( *prev ) );

	/* Mark as inaccessible the area that was temporarily marked
	 * as defined to avoid errors from list_check().
	 */
	VALGRIND_MAKE_MEM_NOACCESS ( &heap->blocks.next->prev,
				     sizeof ( heap->blocks.next->prev ) );

	/* Mark block list itself as inaccessible */
	VALGRIND_MAKE_MEM_NOACCESS ( &heap->blocks, sizeof ( heap->blocks ) );
}

/**
 * Check integrity of the blocks in the free list
 *
 * @v heap		Heap
 */
static inline void check_blocks ( struct heap *heap ) {
	struct memory_block *block;
	struct memory_block *prev = NULL;

	if ( ! ASSERTING )
		return;

	list_for_each_entry ( block, &heap->blocks, list ) {

		/* Check alignment */
		assert ( ( virt_to_phys ( block ) &
			   ( heap->align - 1 ) ) == 0 );

		/* Check that list structure is intact */
		list_check ( &block->list );

		/* Check that block size is not too small */
		assert ( block->size >= sizeof ( *block ) );
		assert ( block->size >= heap->align );

		/* Check that block does not wrap beyond end of address space */
		assert ( ( ( void * ) block + block->size ) >
			 ( ( void * ) block ) );

		/* Check that blocks remain in ascending order, and
		 * that adjacent blocks have been merged.
		 */
		if ( prev ) {
			assert ( ( ( void * ) block ) > ( ( void * ) prev ) );
			assert ( ( ( void * ) block ) >
				 ( ( ( void * ) prev ) + prev->size ) );
		}
		prev = block;
	}
}

/**
 * Discard some cached data
 *
 * @v size		Failed allocation size
 * @ret discarded	Number of cached items discarded
 */
static unsigned int discard_cache ( size_t size __unused ) {
	struct cache_discarder *discarder;
	unsigned int discarded;

	for_each_table_entry ( discarder, CACHE_DISCARDERS ) {
		discarded = discarder->discard();
		if ( discarded )
			return discarded;
	}
	return 0;
}

/**
 * Discard all cached data
 *
 */
static void discard_all_cache ( void ) {
	unsigned int discarded;

	do {
		discarded = discard_cache ( 0 );
	} while ( discarded );
}

/**
 * Allocate a memory block
 *
 * @v heap		Heap
 * @v size		Requested size
 * @v align		Physical alignment
 * @v offset		Offset from physical alignment
 * @ret ptr		Memory block, or NULL
 *
 * Allocates a memory block @b physically aligned as requested.  No
 * guarantees are provided for the alignment of the virtual address.
 *
 * @c align must be a power of two.  @c size may not be zero.
 */
static void * heap_alloc_block ( struct heap *heap, size_t size, size_t align,
				 size_t offset ) {
	struct memory_block *block;
	size_t actual_offset;
	size_t align_mask;
	size_t actual_size;
	size_t pre_size;
	size_t post_size;
	struct memory_block *pre;
	struct memory_block *post;
	unsigned int grown;
	void *ptr;

	/* Sanity checks */
	assert ( size != 0 );
	assert ( ( align == 0 ) || ( ( align & ( align - 1 ) ) == 0 ) );
	valgrind_make_blocks_defined ( heap );
	check_blocks ( heap );

	/* Limit offset to requested alignment */
	offset &= ( align ? ( align - 1 ) : 0 );

	/* Calculate offset of memory block */
	actual_offset = ( offset & ~( heap->align - 1 ) );
	assert ( actual_offset <= offset );

	/* Calculate size of memory block */
	actual_size = ( ( size + offset - actual_offset + heap->align - 1 )
			& ~( heap->align - 1 ) );
	if ( ! actual_size ) {
		/* The requested size is not permitted to be zero.  A
		 * zero result at this point indicates that either the
		 * original requested size was zero, or that unsigned
		 * integer overflow has occurred.
		 */
		ptr = NULL;
		goto done;
	}
	assert ( actual_size >= size );

	/* Calculate alignment mask */
	align_mask = ( ( align - 1 ) | ( heap->align - 1 ) );

	DBGC2 ( heap, "HEAP allocating %#zx (aligned %#zx+%#zx)\n",
		size, align, offset );
	while ( 1 ) {
		/* Search through blocks for the first one with enough space */
		list_for_each_entry ( block, &heap->blocks, list ) {
			pre_size = ( ( actual_offset - virt_to_phys ( block ) )
				     & align_mask );
			if ( ( block->size < pre_size ) ||
			     ( ( block->size - pre_size ) < actual_size ) )
				continue;
			post_size = ( block->size - pre_size - actual_size );
			/* Split block into pre-block, block, and
			 * post-block.  After this split, the "pre"
			 * block is the one currently linked into the
			 * free list.
			 */
			pre   = block;
			block = ( ( ( void * ) pre   ) + pre_size );
			post  = ( ( ( void * ) block ) + actual_size );
			DBGC2 ( heap, "HEAP splitting [%p,%p) -> [%p,%p) "
				"+ [%p,%p)\n", pre,
				( ( ( void * ) pre ) + pre->size ), pre, block,
				post, ( ( ( void * ) pre ) + pre->size ) );
			/* If there is a "post" block, add it in to
			 * the free list.
			 */
			if ( post_size ) {
				assert ( post_size >= sizeof ( *block ) );
				assert ( ( post_size &
					   ( heap->align - 1 ) ) == 0 );
				VALGRIND_MAKE_MEM_UNDEFINED ( post,
							      sizeof ( *post ));
				post->size = post_size;
				list_add ( &post->list, &pre->list );
			}
			/* Shrink "pre" block, leaving the main block
			 * isolated and no longer part of the free
			 * list.
			 */
			pre->size = pre_size;
			/* If there is no "pre" block, remove it from
			 * the list.
			 */
			if ( ! pre_size ) {
				list_del ( &pre->list );
				VALGRIND_MAKE_MEM_NOACCESS ( pre,
							     sizeof ( *pre ) );
			} else {
				assert ( pre_size >= sizeof ( *block ) );
				assert ( ( pre_size &
					   ( heap->align - 1 ) ) == 0 );
			}
			/* Update memory usage statistics */
			heap->freemem -= actual_size;
			heap->usedmem += actual_size;
			if ( heap->usedmem > heap->maxusedmem )
				heap->maxusedmem = heap->usedmem;
			/* Return allocated block */
			ptr = ( ( ( void * ) block ) + offset - actual_offset );
			DBGC2 ( heap, "HEAP allocated [%p,%p) within "
				"[%p,%p)\n", ptr, ( ptr + size ), block,
				( ( ( void * ) block ) + actual_size ) );
			VALGRIND_MAKE_MEM_UNDEFINED ( ptr, size );
			goto done;
		}

		/* Attempt to grow heap to satisfy allocation */
		DBGC ( heap, "HEAP attempting to grow for %#zx (aligned "
		       "%#zx+%zx), used %zdkB\n", size, align, offset,
		       ( heap->usedmem >> 10 ) );
		valgrind_make_blocks_noaccess ( heap );
		grown = ( heap->grow ? heap->grow ( actual_size ) : 0 );
		valgrind_make_blocks_defined ( heap );
		check_blocks ( heap );
		if ( ! grown ) {
			/* Heap did not grow: fail allocation */
			DBGC ( heap, "HEAP failed to allocate %#zx (aligned "
			       "%#zx)\n", size, align );
			ptr = NULL;
			goto done;
		}
	}

 done:
	check_blocks ( heap );
	valgrind_make_blocks_noaccess ( heap );
	return ptr;
}

/**
 * Free a memory block
 *
 * @v heap		Heap
 * @v ptr		Memory allocated by heap_alloc_block(), or NULL
 * @v size		Size of the memory
 *
 * If @c ptr is NULL, no action is taken.
 */
static void heap_free_block ( struct heap *heap, void *ptr, size_t size ) {
	struct memory_block *freeing;
	struct memory_block *block;
	struct memory_block *tmp;
	size_t sub_offset;
	size_t actual_size;
	ssize_t gap_before;
	ssize_t gap_after = -1;

	/* Allow for ptr==NULL */
	if ( ! ptr )
		return;
	VALGRIND_MAKE_MEM_NOACCESS ( ptr, size );

	/* Sanity checks */
	valgrind_make_blocks_defined ( heap );
	check_blocks ( heap );

	/* Round up to match actual block that heap_alloc_block() would
	 * have allocated.
	 */
	assert ( size != 0 );
	sub_offset = ( virt_to_phys ( ptr ) & ( heap->align - 1 ) );
	freeing = ( ptr - sub_offset );
	actual_size = ( ( size + sub_offset + heap->align - 1 ) &
			~( heap->align - 1 ) );
	DBGC2 ( heap, "HEAP freeing [%p,%p) within [%p,%p)\n",
		ptr, ( ptr + size ), freeing,
		( ( ( void * ) freeing ) + actual_size ) );
	VALGRIND_MAKE_MEM_UNDEFINED ( freeing, sizeof ( *freeing ) );

	/* Check that this block does not overlap the free list */
	if ( ASSERTING ) {
		list_for_each_entry ( block, &heap->blocks, list ) {
			if ( ( ( ( void * ) block ) <
			       ( ( void * ) freeing + actual_size ) ) &&
			     ( ( void * ) freeing <
			       ( ( void * ) block + block->size ) ) ) {
				assert ( 0 );
				DBGC ( heap, "HEAP double free of [%p,%p) "
				       "overlapping [%p,%p) detected from %p\n",
				       freeing,
				       ( ( ( void * ) freeing ) + size ), block,
				       ( ( void * ) block + block->size ),
				       __builtin_return_address ( 0 ) );
			}
		}
	}

	/* Insert/merge into free list */
	freeing->size = actual_size;
	list_for_each_entry_safe ( block, tmp, &heap->blocks, list ) {
		/* Calculate gaps before and after the "freeing" block */
		gap_before = ( ( ( void * ) freeing ) - 
			       ( ( ( void * ) block ) + block->size ) );
		gap_after = ( ( ( void * ) block ) - 
			      ( ( ( void * ) freeing ) + freeing->size ) );
		/* Merge with immediately preceding block, if possible */
		if ( gap_before == 0 ) {
			DBGC2 ( heap, "HEAP merging [%p,%p) + [%p,%p) -> "
				"[%p,%p)\n", block,
				( ( ( void * ) block ) + block->size ), freeing,
				( ( ( void * ) freeing ) + freeing->size ),
				block,
				( ( ( void * ) freeing ) + freeing->size ) );
			block->size += actual_size;
			list_del ( &block->list );
			VALGRIND_MAKE_MEM_NOACCESS ( freeing,
						     sizeof ( *freeing ) );
			freeing = block;
		}
		/* Stop processing as soon as we reach a following block */
		if ( gap_after >= 0 )
			break;
	}

	/* Insert before the immediately following block.  If
	 * possible, merge the following block into the "freeing"
	 * block.
	 */
	DBGC2 ( heap, "HEAP freed [%p,%p)\n",
		freeing, ( ( ( void * ) freeing ) + freeing->size ) );
	list_add_tail ( &freeing->list, &block->list );
	if ( gap_after == 0 ) {
		DBGC2 ( heap, "HEAP merging [%p,%p) + [%p,%p) -> [%p,%p)\n",
			freeing, ( ( ( void * ) freeing ) + freeing->size ),
			block, ( ( ( void * ) block ) + block->size ), freeing,
			( ( ( void * ) block ) + block->size ) );
		freeing->size += block->size;
		list_del ( &block->list );
		VALGRIND_MAKE_MEM_NOACCESS ( block, sizeof ( *block ) );
	}

	/* Update memory usage statistics */
	heap->freemem += actual_size;
	heap->usedmem -= actual_size;

	/* Allow heap to shrink */
	if ( heap->shrink && heap->shrink ( freeing, freeing->size ) ) {
		list_del ( &freeing->list );
		heap->freemem -= freeing->size;
		VALGRIND_MAKE_MEM_UNDEFINED ( freeing, freeing->size );
	}

	/* Sanity checks */
	check_blocks ( heap );
	valgrind_make_blocks_noaccess ( heap );
}

/**
 * Reallocate memory
 *
 * @v heap		Heap
 * @v old_ptr		Memory previously allocated by heap_realloc(), or NULL
 * @v new_size		Requested size
 * @ret new_ptr		Allocated memory, or NULL
 *
 * Allocates memory with no particular alignment requirement.  @c
 * new_ptr will be aligned to at least a multiple of sizeof(void*).
 * If @c old_ptr is non-NULL, then the contents of the newly allocated
 * memory will be the same as the contents of the previously allocated
 * memory, up to the minimum of the old and new sizes.  The old memory
 * will be freed.
 *
 * If allocation fails the previously allocated block is left
 * untouched and NULL is returned.
 *
 * Calling heap_realloc() with a new size of zero is a valid way to
 * free a memory block.
 */
void * heap_realloc ( struct heap *heap, void *old_ptr, size_t new_size ) {
	struct autosized_block *old_block;
	struct autosized_block *new_block;
	size_t old_total_size;
	size_t new_total_size;
	size_t old_size;
	size_t offset = offsetof ( struct autosized_block, data );
	void *new_ptr = NOWHERE;

	/* Allocate new memory if necessary.  If allocation fails,
	 * return without touching the old block.
	 */
	if ( new_size ) {
		new_total_size = ( new_size + offset );
		if ( new_total_size < new_size )
			return NULL;
		new_block = heap_alloc_block ( heap, new_total_size,
					       heap->ptr_align, -offset );
		if ( ! new_block )
			return NULL;
		new_block->size = new_total_size;
		VALGRIND_MAKE_MEM_NOACCESS ( &new_block->size,
					     sizeof ( new_block->size ) );
		new_ptr = &new_block->data;
		VALGRIND_MALLOCLIKE_BLOCK ( new_ptr, new_size, 0, 0 );
		assert ( ( ( ( intptr_t ) new_ptr ) &
			   ( heap->ptr_align - 1 ) ) == 0 );
	}

	/* Copy across relevant part of the old data region (if any),
	 * then free it.  Note that at this point either (a) new_ptr
	 * is valid, or (b) new_size is 0; either way, the memcpy() is
	 * valid.
	 */
	if ( old_ptr && ( old_ptr != NOWHERE ) ) {
		old_block = container_of ( old_ptr, struct autosized_block,
					   data );
		VALGRIND_MAKE_MEM_DEFINED ( &old_block->size,
					    sizeof ( old_block->size ) );
		old_total_size = old_block->size;
		assert ( old_total_size != 0 );
		old_size = ( old_total_size - offset );
		memcpy ( new_ptr, old_ptr,
			 ( ( old_size < new_size ) ? old_size : new_size ) );
		VALGRIND_FREELIKE_BLOCK ( old_ptr, 0 );
		heap_free_block ( heap, old_block, old_total_size );
	}

	if ( ASSERTED ) {
		DBGC ( heap, "HEAP detected possible memory corruption "
		       "from %p\n", __builtin_return_address ( 0 ) );
	}
	return new_ptr;
}

/** The global heap */
static struct heap heap = {
	.blocks = LIST_HEAD_INIT ( heap.blocks ),
	.align = MIN_MEMBLOCK_ALIGN,
	.ptr_align = sizeof ( void * ),
	.grow = discard_cache,
};

/**
 * Reallocate memory
 *
 * @v old_ptr		Memory previously allocated by malloc(), or NULL
 * @v new_size		Requested size
 * @ret new_ptr		Allocated memory, or NULL
 */
void * realloc ( void *old_ptr, size_t new_size ) {

	return heap_realloc ( &heap, old_ptr, new_size );
}

/**
 * Allocate memory
 *
 * @v size		Requested size
 * @ret ptr		Memory, or NULL
 *
 * Allocates memory with no particular alignment requirement.  @c ptr
 * will be aligned to at least a multiple of sizeof(void*).
 */
void * malloc ( size_t size ) {
	void *ptr;

	ptr = realloc ( NULL, size );
	if ( ASSERTED ) {
		DBGC ( &heap, "HEAP detected possible memory corruption "
		       "from %p\n", __builtin_return_address ( 0 ) );
	}
	return ptr;
}

/**
 * Free memory
 *
 * @v ptr		Memory allocated by malloc(), or NULL
 *
 * Memory allocated with malloc_phys() cannot be freed with free(); it
 * must be freed with free_phys() instead.
 *
 * If @c ptr is NULL, no action is taken.
 */
void free ( void *ptr ) {

	realloc ( ptr, 0 );
	if ( ASSERTED ) {
		DBGC ( &heap, "HEAP detected possible memory corruption "
		       "from %p\n", __builtin_return_address ( 0 ) );
	}
}

/**
 * Allocate cleared memory
 *
 * @v size		Requested size
 * @ret ptr		Allocated memory
 *
 * Allocate memory as per malloc(), and zero it.
 *
 * This function name is non-standard, but pretty intuitive.
 * zalloc(size) is always equivalent to calloc(1,size)
 */
void * zalloc ( size_t size ) {
	void *data;

	data = malloc ( size );
	if ( data )
		memset ( data, 0, size );
	if ( ASSERTED ) {
		DBGC ( &heap, "HEAP detected possible memory corruption "
		       "from %p\n", __builtin_return_address ( 0 ) );
	}
	return data;
}

/**
 * Allocate memory with specified physical alignment and offset
 *
 * @v size		Requested size
 * @v align		Physical alignment
 * @v offset		Offset from physical alignment
 * @ret ptr		Memory, or NULL
 *
 * @c align must be a power of two.  @c size may not be zero.
 */
void * malloc_phys_offset ( size_t size, size_t phys_align, size_t offset ) {
	void * ptr;

	ptr = heap_alloc_block ( &heap, size, phys_align, offset );
	if ( ptr && size ) {
		assert ( ( phys_align == 0 ) ||
			 ( ( ( virt_to_phys ( ptr ) ^ offset ) &
			     ( phys_align - 1 ) ) == 0 ) );
		VALGRIND_MALLOCLIKE_BLOCK ( ptr, size, 0, 0 );
	}
	return ptr;
}

/**
 * Allocate memory with specified physical alignment
 *
 * @v size		Requested size
 * @v align		Physical alignment
 * @ret ptr		Memory, or NULL
 *
 * @c align must be a power of two.  @c size may not be zero.
 */
void * malloc_phys ( size_t size, size_t phys_align ) {

	return malloc_phys_offset ( size, phys_align, 0 );
}

/**
 * Free memory allocated with malloc_phys()
 *
 * @v ptr		Memory allocated by malloc_phys(), or NULL
 * @v size		Size of memory, as passed to malloc_phys()
 *
 * Memory allocated with malloc_phys() can only be freed with
 * free_phys(); it cannot be freed with the standard free().
 *
 * If @c ptr is NULL, no action is taken.
 */
void free_phys ( void *ptr, size_t size ) {

	VALGRIND_FREELIKE_BLOCK ( ptr, 0 );
	heap_free_block ( &heap, ptr, size );
}

/**
 * Add memory to allocation pool
 *
 * @v heap		Heap
 * @v start		Start address
 * @v len		Length of memory
 *
 * Adds a block of memory to the allocation pool.  The memory must be
 * aligned to the heap's required free memory block alignment.
 */
void heap_populate ( struct heap *heap, void *start, size_t len ) {

	/* Sanity checks */
	assert ( ( virt_to_phys ( start ) & ( heap->align - 1 ) ) == 0 );
	assert ( ( len & ( heap->align - 1 ) ) == 0 );

	/* Add to allocation pool */
	heap_free_block ( heap, start, len );

	/* Fix up memory usage statistics */
	heap->usedmem += len;
}

/**
 * Initialise the heap
 *
 */
static void init_heap ( void ) {

	/* Sanity check */
	build_assert ( MIN_MEMBLOCK_ALIGN >= sizeof ( struct memory_block ) );

	/* Populate heap */
	VALGRIND_MAKE_MEM_NOACCESS ( heap_area, sizeof ( heap_area ) );
	VALGRIND_MAKE_MEM_NOACCESS ( &heap.blocks, sizeof ( heap.blocks ) );
	heap_populate ( &heap, heap_area, sizeof ( heap_area ) );
}

/** Memory allocator initialisation function */
struct init_fn heap_init_fn __init_fn ( INIT_EARLY ) = {
	.initialise = init_heap,
};

/**
 * Discard all cached data on shutdown
 *
 */
static void shutdown_cache ( int booting __unused ) {
	discard_all_cache();
	DBGC ( &heap, "HEAP maximum usage %zdkB\n",
	       ( heap.maxusedmem >> 10 ) );
}

/** Memory allocator shutdown function */
struct startup_fn heap_startup_fn __startup_fn ( STARTUP_EARLY ) = {
	.name = "heap",
	.shutdown = shutdown_cache,
};

/**
 * Dump free block list (for debugging)
 *
 */
void heap_dump ( struct heap *heap ) {
	struct memory_block *block;

	dbg_printf ( "HEAP free block list:\n" );
	list_for_each_entry ( block, &heap->blocks, list ) {
		dbg_printf ( "...[%p,%p] (size %#zx)\n", block,
			     ( ( ( void * ) block ) + block->size ),
			     block->size );
	}
}
