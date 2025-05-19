/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <string.h>
#include <errno.h>
#include <initrd.h>
#include <ipxe/image.h>
#include <ipxe/uaccess.h>
#include <ipxe/init.h>
#include <ipxe/memmap.h>
#include <ipxe/cpio.h>

/** @file
 *
 * Initial ramdisk (initrd) reshuffling
 *
 */

/** Maximum address available for initrd */
static physaddr_t initrd_top;

/** Minimum address available for initrd */
static physaddr_t initrd_bottom;

/**
 * Squash initrds as high as possible in memory
 *
 * @v top		Highest possible physical address
 * @ret used		Lowest physical address used by initrds
 */
static physaddr_t initrd_squash_high ( physaddr_t top ) {
	physaddr_t current = top;
	struct image *initrd;
	struct image *highest;
	void *data;
	size_t len;

	/* Squash up any initrds already within or below the region */
	while ( 1 ) {

		/* Find the highest image not yet in its final position */
		highest = NULL;
		for_each_image ( initrd ) {
			if ( ( virt_to_phys ( initrd->data ) < current ) &&
			     ( ( highest == NULL ) ||
			       ( virt_to_phys ( initrd->data ) >
				 virt_to_phys ( highest->data ) ) ) ) {
				highest = initrd;
			}
		}
		if ( ! highest )
			break;

		/* Move this image to its final position */
		len = ( ( highest->len + INITRD_ALIGN - 1 ) &
			~( INITRD_ALIGN - 1 ) );
		current -= len;
		DBGC ( &images, "INITRD squashing %s [%#08lx,%#08lx)->"
		       "[%#08lx,%#08lx)\n", highest->name,
		       virt_to_phys ( highest->data ),
		       ( virt_to_phys ( highest->data ) + highest->len ),
		       current, ( current + highest->len ) );
		data = phys_to_virt ( current );
		memmove ( data, highest->data, highest->len );
		highest->data = data;
	}

	/* Copy any remaining initrds (e.g. embedded images) to the region */
	for_each_image ( initrd ) {
		if ( virt_to_phys ( initrd->data ) >= top ) {
			len = ( ( initrd->len + INITRD_ALIGN - 1 ) &
				~( INITRD_ALIGN - 1 ) );
			current -= len;
			DBGC ( &images, "INITRD copying %s [%#08lx,%#08lx)->"
			       "[%#08lx,%#08lx)\n", initrd->name,
			       virt_to_phys ( initrd->data ),
			       ( virt_to_phys ( initrd->data ) + initrd->len ),
			       current, ( current + initrd->len ) );
			data = phys_to_virt ( current );
			memcpy ( data, initrd->data, initrd->len );
			initrd->data = data;
		}
	}

	return current;
}

/**
 * Swap position of two adjacent initrds
 *
 * @v low		Lower initrd
 * @v high		Higher initrd
 * @v free		Free space
 * @v free_len		Length of free space
 */
static void initrd_swap ( struct image *low, struct image *high,
			  void *free, size_t free_len ) {
	size_t len = 0;
	size_t frag_len;
	size_t new_len;

	DBGC ( &images, "INITRD swapping %s [%#08lx,%#08lx)<->[%#08lx,%#08lx) "
	       "%s\n", low->name, virt_to_phys ( low->data ),
	       ( virt_to_phys ( low->data ) + low->len ),
	       virt_to_phys ( high->data ),
	       ( virt_to_phys ( high->data ) + high->len ), high->name );

	/* Round down length of free space */
	free_len &= ~( INITRD_ALIGN - 1 );
	assert ( free_len > 0 );

	/* Swap image data */
	while ( len < high->len ) {

		/* Calculate maximum fragment length */
		frag_len = ( high->len - len );
		if ( frag_len > free_len )
			frag_len = free_len;
		new_len = ( ( len + frag_len + INITRD_ALIGN - 1 ) &
			    ~( INITRD_ALIGN - 1 ) );

		/* Swap fragments */
		memcpy ( free, ( high->data + len ), frag_len );
		memmove ( ( low->rwdata + new_len ), ( low->data + len ),
			  low->len );
		memcpy ( ( low->rwdata + len ), free, frag_len );
		len = new_len;
	}

	/* Adjust data pointers */
	high->data = low->data;
	low->data += len;
}

/**
 * Swap position of any two adjacent initrds not currently in the correct order
 *
 * @v free		Free space
 * @v free_len		Length of free space
 * @ret swapped		A pair of initrds was swapped
 */
static int initrd_swap_any ( void *free, size_t free_len ) {
	struct image *low;
	struct image *high;
	const void *adjacent;
	size_t padded_len;

	/* Find any pair of initrds that can be swapped */
	for_each_image ( low ) {

		/* Calculate location of adjacent image (if any) */
		padded_len = ( ( low->len + INITRD_ALIGN - 1 ) &
			       ~( INITRD_ALIGN - 1 ) );
		adjacent = ( low->data + padded_len );

		/* Search for adjacent image */
		for_each_image ( high ) {

			/* Stop search if all remaining potential
			 * adjacent images are already in the correct
			 * order.
			 */
			if ( high == low )
				break;

			/* If we have found the adjacent image, swap and exit */
			if ( high->data == adjacent ) {
				initrd_swap ( low, high, free, free_len );
				return 1;
			}
		}
	}

	/* Nothing swapped */
	return 0;
}

/**
 * Dump initrd locations (for debug)
 *
 */
static void initrd_dump ( void ) {
	struct image *initrd;

	/* Do nothing unless debugging is enabled */
	if ( ! DBG_LOG )
		return;

	/* Dump initrd locations */
	for_each_image ( initrd ) {
		DBGC ( &images, "INITRD %s at [%#08lx,%#08lx)\n",
		       initrd->name, virt_to_phys ( initrd->data ),
		       ( virt_to_phys ( initrd->data ) + initrd->len ) );
		DBGC2_MD5A ( &images, virt_to_phys ( initrd->data ),
			     initrd->data, initrd->len );
	}
}

/**
 * Reshuffle initrds into desired order at top of memory
 *
 * @v bottom		Lowest physical address available for initrds
 *
 * After this function returns, the initrds have been rearranged in
 * memory and the external heap structures will have been corrupted.
 * Reshuffling must therefore take place immediately prior to jumping
 * to the loaded OS kernel; no further execution within iPXE is
 * permitted.
 */
void initrd_reshuffle ( physaddr_t bottom ) {
	physaddr_t top;
	physaddr_t used;
	void *free;
	size_t free_len;

	/* Calculate limits of available space for initrds */
	top = initrd_top;
	if ( initrd_bottom > bottom )
		bottom = initrd_bottom;

	/* Debug */
	DBGC ( &images, "INITRD region [%#08lx,%#08lx)\n", bottom, top );
	initrd_dump();

	/* Squash initrds as high as possible in memory */
	used = initrd_squash_high ( top );

	/* Calculate available free space */
	free = phys_to_virt ( bottom );
	free_len = ( used - bottom );

	/* Bubble-sort initrds into desired order */
	while ( initrd_swap_any ( free, free_len ) ) {}

	/* Debug */
	initrd_dump();
}

/**
 * Check that there is enough space to reshuffle initrds
 *
 * @v len		Total length of initrds (including padding)
 * @v bottom		Lowest physical address available for initrds
 * @ret rc		Return status code
 */
int initrd_reshuffle_check ( size_t len, physaddr_t bottom ) {
	physaddr_t top;
	size_t available;

	/* Calculate limits of available space for initrds */
	top = initrd_top;
	if ( initrd_bottom > bottom )
		bottom = initrd_bottom;
	available = ( top - bottom );

	/* Allow for a sensible minimum amount of free space */
	len += INITRD_MIN_FREE_LEN;

	/* Check for available space */
	return ( ( len < available ) ? 0 : -ENOBUFS );
}

/**
 * initrd startup function
 *
 */
static void initrd_startup ( void ) {
	size_t len;

	/* Record largest memory block available.  Do this after any
	 * allocations made during driver startup (e.g. large host
	 * memory blocks for Infiniband devices, which may still be in
	 * use at the time of rearranging if a SAN device is hooked)
	 * but before any allocations for downloaded images (which we
	 * can safely reuse when rearranging).
	 */
	len = memmap_largest ( &initrd_bottom );
	initrd_top = ( initrd_bottom + len );
	DBGC ( &images, "INITRD largest memory block is [%#08lx,%#08lx)\n",
	       initrd_bottom, initrd_top );
}

/** initrd startup function */
struct startup_fn startup_initrd __startup_fn ( STARTUP_LATE ) = {
	.name = "initrd",
	.startup = initrd_startup,
};
