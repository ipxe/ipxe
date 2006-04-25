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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <io.h>
#include <gpxe/list.h>
#include <malloc.h>

/** @file
 *
 * Dynamic memory allocation
 *
 */

/** A free block of memory */
struct memory_block {
	/** List of free blocks */
	struct list_head list;
	/** Size of this block */
	size_t size;
};

#define MIN_MEMBLOCK_SIZE \
	( ( size_t ) ( 1 << ( fls ( sizeof ( struct memory_block ) - 1 ) ) ) )

/** A block of allocated memory complete with size information */
struct autosized_block {
	/** Size of this block */
	size_t size;
	/** Remaining data */
	char data[0];
};

/** List of free memory blocks */
static LIST_HEAD ( free_blocks );

/**
 * Allocate a memory block
 *
 * @v size		Requested size
 * @v align		Physical alignment
 * @ret ptr		Memory block, or NULL
 *
 * Allocates a memory block @b physically aligned as requested.  No
 * guarantees are provided for the alignment of the virtual address.
 *
 * @c align must be a power of two.  @c size may not be zero.
 */
void * alloc_memblock ( size_t size, size_t align ) {
	struct memory_block *block;
	size_t pre_size;
	ssize_t post_size;
	struct memory_block *pre;
	struct memory_block *post;

	/* Round up alignment and size to multiples of MIN_MEMBLOCK_SIZE */
	align = ( align + MIN_MEMBLOCK_SIZE - 1 ) & ~( MIN_MEMBLOCK_SIZE - 1 );
	size = ( size + MIN_MEMBLOCK_SIZE - 1 ) & ~( MIN_MEMBLOCK_SIZE - 1 );

	/* Search through blocks for the first one with enough space */
	list_for_each_entry ( block, &free_blocks, list ) {
		pre_size = ( - virt_to_phys ( block ) ) & ( align - 1 );
		post_size = block->size - pre_size - size;
		if ( post_size >= 0 ) {
			/* Split block into pre-block, block, and
			 * post-block.  After this split, the "pre"
			 * block is the one currently linked into the
			 * free list.
			 */
			pre   = block;
			block = ( ( ( void * ) pre   ) + pre_size );
			post  = ( ( ( void * ) block ) + size     );
			/* If there is a "post" block, add it in to
			 * the free list.  Leak it if it is too small
			 * (which can happen only at the very end of
			 * the heap).
			 */
			if ( ( size_t ) post_size > MIN_MEMBLOCK_SIZE ) {
				post->size = post_size;
				list_add ( &post->list, &pre->list );
			}
			/* Shrink "pre" block, leaving the main block
			 * isolated and no longer part of the free
			 * list.
			 */
			pre->size = pre_size;
			/* If there is no "pre" block, remove it from
			 * the list.  Also remove it (i.e. leak it) if
			 * it is too small, which can happen only at
			 * the very start of the heap.
			 */
			if ( pre_size < MIN_MEMBLOCK_SIZE )
				list_del ( &pre->list );
			/* Zero allocated memory, for calloc() */
			memset ( block, 0, size );
			return block;
		}
	}
	return NULL;
}

/**
 * Free a memory block
 *
 * @v ptr		Memory allocated by alloc_memblock(), or NULL
 * @v size		Size of the memory
 *
 * If @c ptr is NULL, no action is taken.
 */
void free_memblock ( void *ptr, size_t size ) {
	struct memory_block *freeing;
	struct memory_block *block;
	ssize_t gap_before;
	ssize_t gap_after;

	/* Allow for ptr==NULL */
	if ( ! ptr )
		return;

	/* Round up size to match actual size that alloc_memblock()
	 * would have used.
	 */
	size = ( size + MIN_MEMBLOCK_SIZE - 1 ) & ~( MIN_MEMBLOCK_SIZE - 1 );
	freeing = ptr;
	freeing->size = size;

	/* Insert/merge into free list */
	list_for_each_entry ( block, &free_blocks, list ) {
		/* Calculate gaps before and after the "freeing" block */
		gap_before = ( ( ( void * ) freeing ) - 
			       ( ( ( void * ) block ) + block->size ) );
		gap_after = ( ( ( void * ) block ) - 
			      ( ( ( void * ) freeing ) + freeing->size ) );
		/* Merge with immediately preceding block, if possible */
		if ( gap_before == 0 ) {
			block->size += size;
			list_del ( &block->list );
			freeing = block;
		}
		/* Insert before the immediately following block.  If
		 * possible, merge the following block into the
		 * "freeing" block.
		 */
		if ( gap_after >= 0 ) {
			list_add_tail ( &freeing->list, &block->list );
			if ( gap_after == 0 ) {
				freeing->size += block->size;
				list_del ( &block->list );
			}
			break;
		}
	}
}

/**
 * Allocate memory
 *
 * @v size		Requested size
 * @ret ptr		Memory, or NULL
 *
 * Allocates memory with no particular alignment requirement.  @c ptr
 * will be aligned to at least a multiple of sizeof(void*).
 *
 * Note that malloc() is very inefficient for allocating blocks where
 * the size is a power of two; if you have many of these
 * (e.g. descriptor rings, data buffers) you should use malloc_dma()
 * instead.
 */
void * malloc ( size_t size ) {
	size_t total_size;
	struct autosized_block *block;

	total_size = size + offsetof ( struct autosized_block, data );
	block = alloc_memblock ( total_size, 1 );
	if ( ! block )
		return NULL;
	block->size = size;
	return &block->data;
}

/**
 * Free memory
 *
 * @v size		Memory allocated by malloc(), or NULL
 *
 * Memory allocated with malloc_dma() cannot be freed with free(); it
 * must be freed with free_dma() instead.
 *
 * If @c ptr is NULL, no action is taken.
 */
void free ( void *ptr ) {
	struct autosized_block *block;

	if ( ptr ) {
		block = container_of ( ptr, struct autosized_block, data );
		free_memblock ( block, block->size );
	}
}

/**
 * Add memory to allocation pool
 *
 * @v start		Start address
 * @v end		End address
 *
 * Adds a block of memory [start,end) to the allocation pool.  This is
 * a one-way operation; there is no way to reclaim this memory.
 *
 * @c start must be aligned to at least a multiple of sizeof(void*).
 */
void mpopulate ( void *start, size_t len ) {
	free_memblock ( start, ( len & ~( MIN_MEMBLOCK_SIZE - 1 ) ) );
}

#if 1
#include <vsprintf.h>
/**
 * Dump free block list
 *
 */
void mdumpfree ( void ) {
	struct memory_block *block;

	printf ( "Free block list:\n" );
	list_for_each_entry ( block, &free_blocks, list ) {
		printf ( "[%p,%p] (size %zx)\n", block,
			 ( ( ( void * ) block ) + block->size ), block->size );
	}
}
#endif

