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
	uint64_t last;

	/* Sanity check */
	assert ( region->last >= region->addr );

	/* Ignore empty regions */
	if ( ! size )
		return;

	/* Calculate last addresses (and truncate if necessary) */
	last = ( start + size - 1 );
	if ( last < start ) {
		last = ~( ( uint64_t ) 0 );
		DBGC ( region, "MEMMAP [%#08llx,%#08llx] %s truncated "
		       "(invalid size %#08llx)\n",
		       ( ( unsigned long long ) start ),
		       ( ( unsigned long long ) last ), name,
		       ( ( unsigned long long ) size ) );
	}

	/* Ignore regions entirely below the region of interest */
	if ( last < region->addr )
		return;

	/* Ignore regions entirely above the region of interest */
	if ( start > region->last )
		return;

	/* Update region of interest as applicable */
	if ( start <= region->addr ) {

		/* Record this region as covering the region of interest */
		region->flags |= flags;
		if ( name )
			region->name = name;

		/* Update last address if no closer boundary exists */
		if ( last < region->last )
			region->last = last;

	} else if ( start < region->last ) {

		/* Update last address if no closer boundary exists */
		region->last = ( start - 1 );
	}

	/* Sanity check */
	assert ( region->last >= region->addr );
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

PROVIDE_MEMMAP_INLINE ( null, memmap_describe );
PROVIDE_MEMMAP_INLINE ( null, memmap_sync );
