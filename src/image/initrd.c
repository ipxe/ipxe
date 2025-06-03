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

/** End of reshuffle region */
static physaddr_t initrd_end;

/**
 * Squash initrds as high as possible in memory
 *
 * @v start		Start of reshuffle region
 * @v end		End of reshuffle region
 */
static void initrd_squash_high ( physaddr_t start, physaddr_t end ) {
	physaddr_t current = end;
	struct image *initrd;
	struct image *highest;
	void *data;

	/* Squash up any initrds already within the region */
	while ( 1 ) {

		/* Find the highest image not yet in its final position */
		highest = NULL;
		for_each_image ( initrd ) {
			if ( ( virt_to_phys ( initrd->data ) >= start ) &&
			     ( virt_to_phys ( initrd->data ) < current ) &&
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
			/* Already at (or crossing) end of region */
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
 * @v start		Start of reshuffle region
 * @v end		End of reshuffle region
 * @ret swapped		A pair of initrds was swapped
 */
static int initrd_swap_any ( physaddr_t start, physaddr_t end ) {
	struct image *low;
	struct image *high;
	const void *adjacent;
	physaddr_t addr;

	/* Find any pair of initrds that can be swapped */
	for_each_image ( low ) {

		/* Ignore images wholly outside the reshuffle region */
		addr = virt_to_phys ( low->data );
		if ( ( addr < start ) || ( addr >= end ) )
			continue;

		/* Calculate location of adjacent image (if any) */
		adjacent = ( low->data + initrd_align ( low->len ) );

		/* Search for adjacent image */
		for_each_image ( high ) {

			/* Ignore images wholly outside the reshuffle region */
			addr = virt_to_phys ( high->data );
			if ( ( addr < start ) || ( addr >= end ) )
				continue;

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
 * After this function returns, the initrds have been rearranged in
 * memory and the external heap structures will have been corrupted.
 * Reshuffling must therefore take place immediately prior to jumping
 * to the loaded OS kernel; no further execution within iPXE is
 * permitted.
 */
void initrd_reshuffle ( void ) {
	physaddr_t start;
	physaddr_t end;

	/* Calculate limits of reshuffle region */
	start = uheap_limit;
	end = ( initrd_end ? initrd_end : uheap_end );

	/* Debug */
	initrd_dump();

	/* Squash initrds as high as possible in memory */
	initrd_squash_high ( start, end );

	/* Bubble-sort initrds into desired order */
	while ( initrd_swap_any ( start, end ) ) {}

	/* Debug */
	initrd_dump();
}

/**
 * Load initrd
 *
 * @v initrd		initrd image
 * @v address		Address at which to load, or NULL
 * @ret len		Length of loaded image, excluding zero-padding
 */
static size_t initrd_load ( struct image *initrd, void *address ) {
	const char *filename = cpio_name ( initrd );
	struct cpio_header cpio;
	size_t offset;
	size_t cpio_len;
	size_t len;
	unsigned int i;

	/* Sanity check */
	assert ( ( address == NULL ) ||
		 ( ( virt_to_phys ( address ) & ( INITRD_ALIGN - 1 ) ) == 0 ));

	/* Skip hidden images */
	if ( initrd->flags & IMAGE_HIDDEN )
		return 0;

	/* Determine length of cpio headers for non-prebuilt images */
	len = 0;
	for ( i = 0 ; ( cpio_len = cpio_header ( initrd, i, &cpio ) ) ; i++ )
		len += ( cpio_len + cpio_pad_len ( cpio_len ) );

	/* Copy in initrd image body and construct any cpio headers */
	if ( address ) {
		memmove ( ( address + len ), initrd->data, initrd->len );
		memset ( address, 0, len );
		offset = 0;
		for ( i = 0 ; ( cpio_len = cpio_header ( initrd, i, &cpio ) ) ;
		      i++ ) {
			memcpy ( ( address + offset ), &cpio,
				 sizeof ( cpio ) );
			memcpy ( ( address + offset + sizeof ( cpio ) ),
				 filename, ( cpio_len - sizeof ( cpio ) ) );
			offset += ( cpio_len + cpio_pad_len ( cpio_len ) );
		}
		assert ( offset == len );
		DBGC ( &images, "INITRD %s [%#08lx,%#08lx,%#08lx)%s%s\n",
		       initrd->name, virt_to_phys ( address ),
		       ( virt_to_phys ( address ) + offset ),
		       ( virt_to_phys ( address ) + offset + initrd->len ),
		       ( filename ? " " : "" ), ( filename ? filename : "" ) );
		DBGC2_MD5A ( &images, ( virt_to_phys ( address ) + offset ),
			     ( address + offset ), initrd->len );
	}
	len += initrd->len;

	return len;
}

/**
 * Load all initrds
 *
 * @v address		Load address, or NULL
 * @ret len		Length
 *
 * This function is called after the point of no return, when the
 * external heap has been corrupted by reshuffling and there is no way
 * to resume normal execution.  The caller must have previously
 * ensured that there is no way for installation to this address to
 * fail.
 */
size_t initrd_load_all ( void *address ) {
	struct image *initrd;
	size_t len = 0;
	size_t pad_len;
	void *dest;

	/* Load all initrds */
	for_each_image ( initrd ) {

		/* Zero-pad to next INITRD_ALIGN boundary */
		pad_len = ( ( -len ) & ( INITRD_ALIGN - 1 ) );
		if ( address )
			memset ( ( address + len ), 0, pad_len );
		len += pad_len;
		assert ( len == initrd_align ( len ) );

		/* Load initrd */
		dest = ( address ? ( address + len ) : NULL );
		len += initrd_load ( initrd, dest );
	}

	return len;
}

/**
 * Calculate post-reshuffle initrd load region
 *
 * @v len		Length of initrds (from initrd_len())
 * @v region		Region descriptor to fill in
 * @ret rc		Return status code
 *
 * If successful, then any suitably aligned range within the region
 * may be used as the load address after reshuffling.  The caller does
 * not need to call prep_segment() for a range in this region.
 * (Calling prep_segment() would probably fail, since prior to
 * reshuffling the region is still in use by the external heap.)
 */
int initrd_region ( size_t len, struct memmap_region *region ) {
	physaddr_t min;
	size_t available;

	/* Calculate limits of available space for initrds */
	min = uheap_limit;
	available = ( ( initrd_end ? initrd_end : uheap_end ) - min );
	if ( available < len )
		return -ENOSPC;
	DBGC ( &images, "INITRD post-reshuffle region is [%#08lx,%#08lx)\n",
	       min, ( min + available ) );

	/* Populate region descriptor */
	region->min = min;
	region->max = ( min + available - 1 );
	region->flags = MEMMAP_FL_MEMORY;
	region->name = "initrd";

	return 0;
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
	initrd_end = uheap_start;
	if ( initrd_end ) {
		DBGC ( &images, "INITRD limiting reshuffling to below "
		       "%#08lx\n", initrd_end );
	}
}

/** initrd startup function */
struct startup_fn startup_initrd __startup_fn ( STARTUP_LATE ) = {
	.name = "initrd",
	.startup = initrd_startup,
};
