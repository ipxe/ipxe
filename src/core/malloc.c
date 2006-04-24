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

#include <stdint.h>
#include <string.h>
#include <io.h>
#include <gpxe/list.h>
#include <gpxe/malloc.h>

/** @file
 *
 * Memory allocation
 *
 */

/** A free block of memory */
struct free_block {
	/** List of free blocks */
	struct list_head list;
	/** Size of this block */
	size_t size;
};

/** List of free memory blocks */
static LIST_HEAD ( free_blocks );

/**
 * Round size up to a memory allocation block size
 *
 * @v requested		Requested size
 * @ret obtained	Obtained size
 *
 * The requested size is rounded up to the minimum allocation block
 * size (the size of a struct @c free_block) and then rounded up to
 * the nearest power of two.
 */
static size_t block_size ( size_t requested ) {
	size_t obtained = 1;

	while ( ( obtained < sizeof ( struct free_block ) ) ||
		( obtained < requested ) ) {
		obtained <<= 1;
	}
	return obtained;
}

/**
 * Allocate memory
 *
 * @v size		Requested size
 * @ret ptr		Allocated memory
 *
 * gmalloc() will always allocate memory in power-of-two sized blocks,
 * aligned to the corresponding power-of-two boundary.  For example, a
 * request for 1500 bytes will return a 2048-byte block aligned to a
 * 2048-byte boundary.
 *
 * The alignment applies to the physical address, not the virtual
 * address.  The pointer value returned by gmalloc() therefore has no
 * alignment guarantees, except as provided for by the
 * virtual-to-physical mapping.  (In a PXE environment, this mapping
 * is guaranteed to be a multiple of 16 bytes.)
 *
 * Unlike traditional malloc(), the caller must remember the size of
 * the allocated block and pass the size to gfree().  This is done in
 * order to allow efficient allocation of power-of-two sized and
 * aligned blocks.
 */
void * gmalloc ( size_t size ) {
	struct free_block *block;
	struct free_block *buddy;

	/* Round up block size to power of two */
	size = block_size ( size );

	/* Find the best available block */
	list_for_each_entry ( block, &free_blocks, list ) {
		if ( block->size == size ) {
			list_del ( &block->list );
			memset ( block, 0, size );
			return block;
		}
		while ( block->size > size ) {
			block->size >>= 1;
			buddy = ( ( ( void * ) block ) + block->size );
			buddy->size = block->size;
			list_add ( &buddy->list, &block->list );
		}
	}

	/* Nothing available */
	return NULL;
}

/**
 * Free memory
 *
 * @v ptr		Allocated memory
 * @v size		Originally requested size
 *
 * Frees memory originally allocated by gmalloc().
 *
 * Calling gfree() with a NULL @c ptr is explicitly allowed, and
 * defined to have no effect.  Code such as
 *
 * @code
 *
 * if ( ! my_ptr )
 *     gfree ( my_ptr, my_size )
 *
 * @endcode
 *
 * is perfectly valid, but should be avoided as unnecessary bloat.
 */
void gfree ( void *ptr, size_t size ) {
	struct free_block *freed_block = ptr;
	struct free_block *block;
	
	/* Cope with gfree(NULL,x) */
	if ( ! ptr )
		return;

	/* Round up block size to power of two */
	size = block_size ( size );
	freed_block->size = size;

	/* Merge back into free list */
	list_for_each_entry ( block, &free_blocks, list ) {
		if ( ( ( virt_to_phys ( block ) ^
			 virt_to_phys ( freed_block ) ) == size ) &&
		     ( block->size == size ) ) {
			list_del ( &block->list );
			size <<= 1;
			if ( block < freed_block )
				freed_block = block;
			freed_block->size = size;
		} else if ( block->size > size ) {
			break;
		}
	}
	list_add_tail ( &freed_block->list, &block->list );
}

/**
 * Add memory to allocation pool
 *
 * @v start		Start address
 * @v len		Length
 *
 * Adds a block of memory to the allocation pool.  This is a one-way
 * operation; there is no way to reclaim this memory.
 *
 * There are no alignment requirements on either start or len.
 */
void gmpopulate ( void *start, size_t len ) {
	size_t frag_len;

	/* Split region into power-of-two sized and aligned blocks,
	 * and feed them to gfree().
	 */
	while ( len ) {
		frag_len = 1;
		/* Find maximum allowed alignment for this address */
		while ( ( virt_to_phys ( start ) & frag_len ) == 0 ) { 
			frag_len <<= 1;
		}
		/* Find maximum block size that fits in remaining space */
		while ( frag_len > len ) {
			frag_len >>= 1;
		}
		/* Skip blocks that are too small */
		if ( frag_len >= sizeof ( struct free_block ) )
			gfree ( start, frag_len );
		start += frag_len;
		len -= frag_len;
	}
}

#if 0
#include <vsprintf.h>
/**
 * Dump free block list
 *
 */
void gdumpfree ( void ) {
	struct free_block *block;

	printf ( "Free block list:\n" );
	list_for_each_entry ( block, &free_blocks, list ) {
		printf ( "[%p,%p] (size %zx)\n", block,
			 ( ( ( void * ) block ) + block->size ), block->size );
	}
}
#endif
