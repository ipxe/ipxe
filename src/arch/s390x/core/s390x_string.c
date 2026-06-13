/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
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

/** @file
 *
 * Optimised string operations
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <string.h>

/**
 * Copy (possibly overlapping) memory region
 *
 * @v dest		Destination region
 * @v src		Source region
 * @v len		Length
 */
void s390x_memmove ( void *dest, const void *src, size_t len ) {
	const uint8_t *src_bytes = ( src + len );
	uint8_t *dest_bytes = ( dest + len );

	/* Use memcpy() if copy direction is forwards */
	if ( dest <= src ) {
		memcpy ( dest, src, len );
		return;
	}

	/* Assume memmove() is not performance-critical, and perform a
	 * bytewise copy backwards for simplicity.
	 */
	while ( len-- )
		*(--dest_bytes) = *(--src_bytes);
}
