/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <ipxe/io.h>
#include <ipxe/memmap.h>
#include <ipxe/malloc.h>
#include <ipxe/umalloc.h>

/** @file
 *
 * External ("user") heap
 *
 * This file implements an external heap (for umalloc()) that grows
 * downwards from the top of the largest contiguous accessible block
 * in the system memory map.
 */

/**
 * Alignment for external heap allocations
 *
 * Historically, umalloc() has produced page-aligned allocations, and
 * the hidden region in the system memory map has been aligned to a
 * page boundary.  Preserve this behaviour, to avoid needing to
 * inspect and update large amounts of driver code, and also because
 * it keeps the resulting memory maps easy to read.
 */
#define UHEAP_ALIGN PAGE_SIZE

static struct heap uheap;

/** In-use memory region */
struct used_region uheap_used __used_region = {
	.name = "uheap",
};

/** External heap maximum size */
static size_t uheap_max;

/**
 * Adjust size of external heap in-use memory region
 *
 * @v delta		Size change
 */
static void uheap_resize ( ssize_t delta ) {
	physaddr_t top;

	/* Update in-use memory region */
	assert ( ( uheap_used.start & ( UHEAP_ALIGN - 1 ) ) == 0 );
	assert ( ( uheap_used.size & ( UHEAP_ALIGN - 1 ) ) == 0 );
	assert ( ( delta & ( UHEAP_ALIGN - 1 ) ) == 0 );
	memmap_use ( &uheap_used, ( uheap_used.start - delta ),
		     ( uheap_used.size + delta ) );
	top = ( uheap_used.start + uheap_used.size );
	DBGC ( &uheap, "UHEAP now at [%#08lx,%#08lx)\n",
	       uheap_used.start, top );
	memmap_dump_all ( 1 );
}

/**
 * Find an external heap region
 *
 */
static void uheap_find ( void ) {
	physaddr_t start;
	physaddr_t end;
	size_t before;
	size_t after;
	size_t strip;
	size_t size;

	/* Sanity checks */
	assert ( uheap_used.size == 0 );
	assert ( uheap_max == 0 );

	/* Find the largest region within the system memory map */
	size = memmap_largest ( &start );
	end = ( start + size );
	DBGC ( &uheap, "UHEAP largest region is [%#08lx,%#08lx)\n",
	       start, end );

	/* Align start and end addresses, and prevent overflow to zero */
	after = ( end ? ( end & ( UHEAP_ALIGN - 1 ) ) : UHEAP_ALIGN );
	before = ( ( -start ) & ( UHEAP_ALIGN - 1 ) );
	strip = ( before + after );
	if ( strip > size )
		return;
	start += before;
	end -= after;
	size -= strip;
	assert ( ( end - start ) == size );

	/* Record region */
	assert ( ( start & ( UHEAP_ALIGN - 1 ) ) == 0 );
	assert ( ( size & ( UHEAP_ALIGN - 1 ) ) == 0 );
	assert ( ( end & ( UHEAP_ALIGN - 1 ) ) == 0 );
	uheap_max = size;
	uheap_used.start = end;
	DBGC ( &uheap, "UHEAP grows downwards from %#08lx\n", end );
}

/**
 * Attempt to grow external heap
 *
 * @v size		Failed allocation size
 * @ret grown		Heap has grown: retry allocations
 */
static unsigned int uheap_grow ( size_t size ) {
	void *new;

	/* Initialise heap, if it does not yet exist */
	if ( ! uheap_max )
		uheap_find();

	/* Fail if insufficient space remains */
	if ( size > ( uheap_max - uheap_used.size ) )
		return 0;

	/* Grow heap */
	new = ( phys_to_virt ( uheap_used.start ) - size );
	heap_populate ( &uheap, new, size );
	uheap_resize ( size );

	return 1;
}

/**
 * Allow external heap to shrink
 *
 * @v ptr		Start of free block
 * @v size		Size of free block
 * @ret shrunk		Heap has shrunk: discard block
 */
static unsigned int uheap_shrink ( void *ptr, size_t size ) {

	/* Do nothing unless this is the lowest block in the heap */
	if ( virt_to_phys ( ptr ) != uheap_used.start )
		return 0;

	/* Shrink heap */
	uheap_resize ( -size );

	return 1;
}

/** The external heap */
static struct heap uheap = {
	.blocks = LIST_HEAD_INIT ( uheap.blocks ),
	.align = UHEAP_ALIGN,
	.ptr_align = UHEAP_ALIGN,
	.grow = uheap_grow,
	.shrink = uheap_shrink,
};

/**
 * Reallocate external memory
 *
 * @v old_ptr		Memory previously allocated by umalloc(), or NULL
 * @v new_size		Requested size
 * @ret new_ptr		Allocated memory, or NULL
 *
 * Calling urealloc() with a new size of zero is a valid way to free a
 * memory block.
 */
static void * uheap_realloc ( void *old_ptr, size_t new_size ) {

	return heap_realloc ( &uheap, old_ptr, new_size );
}

PROVIDE_UMALLOC ( uheap, urealloc, uheap_realloc );
