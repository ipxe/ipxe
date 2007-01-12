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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**
 * @file
 *
 * Automatically expanding buffers
 *
 */

#include <errno.h>
#include <gpxe/buffer.h>
#include <gpxe/emalloc.h>
#include <gpxe/ebuffer.h>

/**
 * Expand expandable buffer
 *
 * @v buffer		Buffer descriptor
 * @v new_len		Required new size
 * @ret rc		Return status code
 */
static int ebuffer_expand ( struct buffer *buffer, size_t new_len ) {
	size_t actual_len = 1;
	userptr_t new_addr;

	/* Round new_len up to the nearest power of two, to reduce
	 * total number of reallocations required.
	 */
	while ( actual_len < new_len )
		actual_len <<= 1;

	/* Reallocate buffer */
	new_addr = erealloc ( buffer->addr, actual_len );
	if ( ! new_addr )
		return -ENOMEM;

	buffer->addr = new_addr;
	buffer->len = actual_len;
	return 0;
}

/**
 * Allocate expandable buffer
 *
 * @v buffer		Buffer descriptor
 * @v len		Initial length (may be zero)
 * @ret rc		Return status code
 *
 * Allocates space for the buffer and stores it in @c buffer->addr.
 * The space must eventually be freed by calling efree(buffer->addr).
 */
int ebuffer_alloc ( struct buffer *buffer, size_t len ) {
	memset ( buffer, 0, sizeof ( *buffer ) );
	buffer->expand = ebuffer_expand;
	return ebuffer_expand ( buffer, len );
}
