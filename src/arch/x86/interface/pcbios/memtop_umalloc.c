/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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

/**
 * @file
 *
 * External memory allocation
 *
 */

#include <limits.h>
#include <errno.h>
#include <ipxe/uaccess.h>
#include <ipxe/hidemem.h>
#include <ipxe/io.h>
#include <ipxe/memblock.h>
#include <ipxe/umalloc.h>

/** Maximum usable address for external allocated memory */
#define EM_MAX_ADDRESS 0xffffffffUL

/** Alignment of external allocated memory */
#define EM_ALIGN ( 4 * 1024 )

/** An external memory block */
struct external_memory {
	/** Size of this memory block (excluding this header) */
	size_t size;
	/** Block is currently in use */
	int used;
};

/** Top of heap */
static void *top = NULL;

/** Bottom of heap (current lowest allocated block) */
static void *bottom = NULL;

/** Remaining space on heap */
static size_t heap_size;

/**
 * Find largest usable memory region
 *
 * @ret start		Start of region
 * @ret len		Length of region
 */
size_t largest_memblock ( void **start ) {
	struct memory_map memmap;
	struct memory_region *region;
	physaddr_t max = EM_MAX_ADDRESS;
	physaddr_t region_start;
	physaddr_t region_end;
	size_t region_len;
	unsigned int i;
	size_t len = 0;

	/* Avoid returning uninitialised data on error */
	*start = NULL;

	/* Scan through all memory regions */
	get_memmap ( &memmap );
	for ( i = 0 ; i < memmap.count ; i++ ) {
		region = &memmap.regions[i];
		DBG ( "Considering [%llx,%llx)\n", region->start, region->end );

		/* Truncate block to maximum physical address */
		if ( region->start > max ) {
			DBG ( "...starts after maximum address %lx\n", max );
			continue;
		}
		region_start = region->start;
		if ( region->end > max ) {
			DBG ( "...end truncated to maximum address %lx\n", max);
			region_end = 0; /* =max, given the wraparound */
		} else {
			region_end = region->end;
		}
		region_len = ( region_end - region_start );

		/* Use largest block */
		if ( region_len > len ) {
			DBG ( "...new best block found\n" );
			*start = phys_to_virt ( region_start );
			len = region_len;
		}
	}

	return len;
}

/**
 * Initialise external heap
 *
 */
static void init_eheap ( void ) {
	void *base;

	heap_size = largest_memblock ( &base );
	bottom = top = ( base + heap_size );
	DBG ( "External heap grows downwards from %lx (size %zx)\n",
	      virt_to_phys ( top ), heap_size );
}

/**
 * Collect free blocks
 *
 */
static void ecollect_free ( void ) {
	struct external_memory extmem;
	size_t len;

	/* Walk the free list and collect empty blocks */
	while ( bottom != top ) {
		memcpy ( &extmem, ( bottom - sizeof ( extmem ) ),
			 sizeof ( extmem ) );
		if ( extmem.used )
			break;
		DBG ( "EXTMEM freeing [%lx,%lx)\n", virt_to_phys ( bottom ),
		      ( virt_to_phys ( bottom ) + extmem.size ) );
		len = ( extmem.size + sizeof ( extmem ) );
		bottom += len;
		heap_size += len;
	}
}

/**
 * Reallocate external memory
 *
 * @v old_ptr		Memory previously allocated by umalloc(), or NULL
 * @v new_size		Requested size
 * @ret new_ptr		Allocated memory, or NULL
 *
 * Calling realloc() with a new size of zero is a valid way to free a
 * memory block.
 */
static void * memtop_urealloc ( void *ptr, size_t new_size ) {
	struct external_memory extmem;
	void *new = ptr;
	size_t align;

	/* (Re)initialise external memory allocator if necessary */
	if ( bottom == top )
		init_eheap();

	/* Get block properties into extmem */
	if ( ptr && ( ptr != NOWHERE ) ) {
		/* Determine old size */
		memcpy ( &extmem, ( ptr - sizeof ( extmem ) ),
			 sizeof ( extmem ) );
	} else {
		/* Create a zero-length block */
		if ( heap_size < sizeof ( extmem ) ) {
			DBG ( "EXTMEM out of space\n" );
			return NULL;
		}
		ptr = bottom = ( bottom - sizeof ( extmem ) );
		heap_size -= sizeof ( extmem );
		DBG ( "EXTMEM allocating [%lx,%lx)\n",
		      virt_to_phys ( ptr ), virt_to_phys ( ptr ) );
		extmem.size = 0;
	}
	extmem.used = ( new_size > 0 );

	/* Expand/shrink block if possible */
	if ( ptr == bottom ) {
		/* Update block */
		new = ( ptr - ( new_size - extmem.size ) );
		align = ( virt_to_phys ( new ) & ( EM_ALIGN - 1 ) );
		new_size += align;
		new -= align;
		if ( new_size > ( heap_size + extmem.size ) ) {
			DBG ( "EXTMEM out of space\n" );
			return NULL;
		}
		DBG ( "EXTMEM expanding [%lx,%lx) to [%lx,%lx)\n",
		      virt_to_phys ( ptr ),
		      ( virt_to_phys ( ptr ) + extmem.size ),
		      virt_to_phys ( new ),
		      ( virt_to_phys ( new ) + new_size ) );
		memmove ( new, ptr, ( ( extmem.size < new_size ) ?
				      extmem.size : new_size ) );
		bottom = new;
		heap_size -= ( new_size - extmem.size );
		extmem.size = new_size;
	} else {
		/* Cannot expand; can only pretend to shrink */
		if ( new_size > extmem.size ) {
			/* Refuse to expand */
			DBG ( "EXTMEM cannot expand [%lx,%lx)\n",
			      virt_to_phys ( ptr ),
			      ( virt_to_phys ( ptr ) + extmem.size ) );
			return NULL;
		}
	}

	/* Write back block properties */
	memcpy ( ( new - sizeof ( extmem ) ), &extmem, sizeof ( extmem ) );

	/* Collect any free blocks and update hidden memory region */
	ecollect_free();
	hide_umalloc ( ( virt_to_phys ( bottom ) -
			 ( ( bottom == top ) ? 0 : sizeof ( extmem ) ) ),
		       virt_to_phys ( top ) );

	return ( new_size ? new : NOWHERE );
}

PROVIDE_UMALLOC ( memtop, urealloc, memtop_urealloc );
