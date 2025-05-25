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
 * Executable image segments
 *
 */

#include <string.h>
#include <errno.h>
#include <ipxe/uaccess.h>
#include <ipxe/memmap.h>
#include <ipxe/errortab.h>
#include <ipxe/segment.h>

/**
 * Segment-specific error messages
 *
 * This error happens sufficiently often to merit a user-friendly
 * description.
 */
#define ERANGE_SEGMENT __einfo_error ( EINFO_ERANGE_SEGMENT )
#define EINFO_ERANGE_SEGMENT \
	__einfo_uniqify ( EINFO_ERANGE, 0x01, "Requested memory not available" )
struct errortab segment_errors[] __errortab = {
	__einfo_errortab ( EINFO_ERANGE_SEGMENT ),
};

/**
 * Prepare segment for loading
 *
 * @v segment		Segment start
 * @v filesz		Size of the "allocated bytes" portion of the segment
 * @v memsz		Size of the segment
 * @ret rc		Return status code
 */
int prep_segment ( void *segment, size_t filesz, size_t memsz ) {
	struct memmap_region region;
	physaddr_t start = virt_to_phys ( segment );
	physaddr_t mid = ( start + filesz );
	physaddr_t end = ( start + memsz );
	physaddr_t max;

	DBGC ( segment, "SEGMENT [%#08lx,%#08lx,%#08lx)\n", start, mid, end );

	/* Check for malformed lengths */
	if ( filesz > memsz ) {
		DBGC ( segment, "SEGMENT [%#08lx,%#08lx,%#08lx) is "
		       "malformed\n", start, mid, end );
		return -EINVAL;
	}

	/* Zero-length segments do not need a memory region */
	if ( memsz == 0 )
		return 0;
	max = ( end - 1 );

	/* Check for address space overflow */
	if ( max < start ) {
		DBGC ( segment, "SEGMENT [%#08lx,%#08lx,%#08lx) wraps "
		       "around\n", start, mid, end );
		return -EINVAL;
	}

	/* Describe region containing this segment */
	memmap_describe ( start, 1, &region );
	DBGC_MEMMAP ( segment, &region );

	/* Fail unless region is usable and sufficiently large */
	if ( ( ! memmap_is_usable ( &region ) ) || ( region.max < max ) ) {
		DBGC ( segment, "SEGMENT [%#08lx,%#08lx,%#08lx) does not fit "
		       "into available memory\n", start, mid, end );
		return -ERANGE_SEGMENT;
	}

	/* Found valid region: zero bss and return */
	memset ( ( segment + filesz ), 0, ( memsz - filesz ) );
	return 0;
}
