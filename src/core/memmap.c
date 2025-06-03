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

#include <assert.h>
#include <ipxe/io.h>
#include <ipxe/memmap.h>

/** @file
 *
 * System memory map
 *
 */

/**
 * Update memory region descriptor
 *
 * @v region		Memory region of interest to be updated
 * @v start		Start address of known region
 * @v size		Size of known region
 * @v flags		Flags for known region
 * @v name		Name of known region (for debugging)
 *
 * Update a memory region descriptor based on a known existent region.
 */
void memmap_update ( struct memmap_region *region, uint64_t start,
		     uint64_t size, unsigned int flags, const char *name ) {
	uint64_t max;

	/* Sanity check */
	assert ( region->max >= region->min );

	/* Ignore empty regions */
	if ( ! size )
		return;

	/* Calculate max addresses (and truncate if necessary) */
	max = ( start + size - 1 );
	if ( max < start ) {
		max = ~( ( uint64_t ) 0 );
		DBGC ( region, "MEMMAP [%#08llx,%#08llx] %s truncated "
		       "(invalid size %#08llx)\n",
		       ( ( unsigned long long ) start ),
		       ( ( unsigned long long ) max ), name,
		       ( ( unsigned long long ) size ) );
	}

	/* Ignore regions entirely below the region of interest */
	if ( max < region->min )
		return;

	/* Ignore regions entirely above the region of interest */
	if ( start > region->max )
		return;

	/* Update region of interest as applicable */
	if ( start <= region->min ) {

		/* Record this region as covering the region of interest */
		region->flags |= flags;
		if ( name )
			region->name = name;

		/* Update max address if no closer boundary exists */
		if ( max < region->max )
			region->max = max;

	} else if ( start < region->max ) {

		/* Update max address if no closer boundary exists */
		region->max = ( start - 1 );
	}

	/* Sanity check */
	assert ( region->max >= region->min );
}

/**
 * Update memory region descriptor based on all in-use memory regions
 *
 * @v region		Memory region of interest to be updated
 */
void memmap_update_used ( struct memmap_region *region ) {
	struct used_region *used;

	/* Update descriptor to hide all in-use regions */
	for_each_table_entry ( used, USED_REGIONS ) {
		memmap_update ( region, used->start, used->size,
				MEMMAP_FL_USED, used->name );
	}
}

/**
 * Find largest usable memory region
 *
 * @v start		Start address to fill in
 * @ret len		Length of region
 */
size_t memmap_largest ( physaddr_t *start ) {
	struct memmap_region region;
	size_t largest;
	size_t size;

	/* Find largest usable region */
	DBGC ( &region, "MEMMAP finding largest usable region\n" );
	*start = 0;
	largest = 0;
	for_each_memmap ( &region, 1 ) {
		DBGC_MEMMAP ( &region, &region );
		if ( ! memmap_is_usable ( &region ) )
			continue;
		size = memmap_size ( &region );
		if ( size > largest ) {
			DBGC ( &region, "...new largest region found\n" );
			largest = size;
			*start = region.min;
		}
	}
	return largest;
}

PROVIDE_MEMMAP_INLINE ( null, memmap_describe );
PROVIDE_MEMMAP_INLINE ( null, memmap_sync );
