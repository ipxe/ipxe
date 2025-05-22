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
#include <ipxe/image.h>
#include <ipxe/uaccess.h>
#include <ipxe/init.h>
#include <ipxe/cpio.h>
#include <ipxe/uheap.h>
#include <ipxe/initrd.h>

/** @file
 *
 * Initial ramdisk (initrd) reshuffling
 *
 */

/** Maximum address available for initrd */
static physaddr_t initrd_top;

/**
 * Squash initrds as high as possible in memory
 *
 * @v top		Highest possible physical address
 */
static void initrd_squash_high ( physaddr_t top ) {
	physaddr_t current = top;
	struct image *initrd;
	struct image *highest;
	void *data;

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

		/* Calculate final position */
		current -= initrd_align ( highest->len );
		if ( current <= virt_to_phys ( highest->data ) ) {
			/* Already at (or crossing) top of region */
			current = virt_to_phys ( highest->data );
			continue;
		}

		/* Move this image to its final position */
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
			current -= initrd_align ( initrd->len );
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
}

/**
 * Reverse aligned memory region
 *
 * @v data		Memory region
 * @v len		Length of region
 */
static void initrd_reverse ( void *data, size_t len ) {
	unsigned long *low = data;
	unsigned long *high = ( data + len );
	unsigned long tmp;

	/* Reverse region */
	for ( high-- ; low < high ; low++, high-- ) {
		tmp = *low;
		*low = *high;
		*high = tmp;
	}
}

/**
 * Swap position of two adjacent initrds
 *
 * @v low		Lower initrd
 * @v high		Higher initrd
 */
static void initrd_swap ( struct image *low, struct image *high ) {
	size_t low_len;
	size_t high_len;
	size_t len;
	void *data;

	DBGC ( &images, "INITRD swapping %s [%#08lx,%#08lx)<->[%#08lx,%#08lx) "
	       "%s\n", low->name, virt_to_phys ( low->data ),
	       ( virt_to_phys ( low->data ) + low->len ),
	       virt_to_phys ( high->data ),
	       ( virt_to_phys ( high->data ) + high->len ), high->name );

	/* Calculate padded lengths */
	low_len = initrd_align ( low->len );
	high_len = initrd_align ( high->len );
	len = ( low_len + high_len );
	data = low->rwdata;
	assert ( high->data == ( data + low_len ) );

	/* Adjust data pointers */
	high->data -= low_len;
	low->data += high_len;
	assert ( high->data == data );

	/* Swap content via triple reversal */
	initrd_reverse ( data, len );
	initrd_reverse ( low->rwdata, low_len );
	initrd_reverse ( high->rwdata, high_len );
}

/**
 * Swap position of any two adjacent initrds not currently in the correct order
 *
 * @ret swapped		A pair of initrds was swapped
 */
static int initrd_swap_any ( void ) {
	struct image *low;
	struct image *high;
	const void *adjacent;

	/* Find any pair of initrds that can be swapped */
	for_each_image ( low ) {

		/* Calculate location of adjacent image (if any) */
		adjacent = ( low->data + initrd_align ( low->len ) );

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
				initrd_swap ( low, high );
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

	/* Calculate limits of available space for initrds */
	top = ( initrd_top ? initrd_top : uheap_end );
	assert ( bottom >= uheap_limit );

	/* Debug */
	DBGC ( &images, "INITRD region [%#08lx,%#08lx)\n", bottom, top );
	initrd_dump();

	/* Squash initrds as high as possible in memory */
	initrd_squash_high ( top );

	/* Bubble-sort initrds into desired order */
	while ( initrd_swap_any() ) {}

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
	top = ( initrd_top ? initrd_top : uheap_end );
	assert ( bottom >= uheap_limit );
	available = ( top - bottom );

	/* Check for available space */
	return ( ( len < available ) ? 0 : -ENOBUFS );
}

/**
 * initrd startup function
 *
 */
static void initrd_startup ( void ) {

	/* Record address above which reshuffling cannot take place.
	 * If any external heap allocations have been made during
	 * driver startup (e.g. large host memory blocks for
	 * Infiniband devices, which may still be in use at the time
	 * of rearranging if a SAN device is hooked), then we must not
	 * overwrite these allocations during reshuffling.
	 */
	initrd_top = uheap_start;
	if ( initrd_top ) {
		DBGC ( &images, "INITRD limiting reshuffling to below "
		       "%#08lx\n", initrd_top );
	}
}

/** initrd startup function */
struct startup_fn startup_initrd __startup_fn ( STARTUP_LATE ) = {
	.name = "initrd",
	.startup = initrd_startup,
};
