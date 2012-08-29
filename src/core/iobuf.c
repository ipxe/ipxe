/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <strings.h>
#include <errno.h>
#include <ipxe/malloc.h>
#include <ipxe/iobuf.h>

/** @file
 *
 * I/O buffers
 *
 */

/**
 * Allocate I/O buffer with specified alignment and offset
 *
 * @v len	Required length of buffer
 * @v align	Physical alignment
 * @v offset	Offset from physical alignment
 * @ret iobuf	I/O buffer, or NULL if none available
 *
 * @c align will be rounded up to the nearest power of two.
 */
struct io_buffer * alloc_iob_raw ( size_t len, size_t align, size_t offset ) {
	struct io_buffer *iobuf;
	void *data;

	/* Align buffer length to ensure that struct io_buffer is aligned */
	len = ( len + __alignof__ ( *iobuf ) - 1 ) &
		~( __alignof__ ( *iobuf ) - 1 );

	/* Round up alignment to the nearest power of two */
	align = ( 1 << fls ( align - 1 ) );

	/* Allocate buffer plus descriptor as a single unit, unless
	 * doing so will push the total size over the alignment
	 * boundary.
	 */
	if ( ( len + sizeof ( *iobuf ) ) <= align ) {

		/* Allocate memory for buffer plus descriptor */
		data = malloc_dma_offset ( len + sizeof ( *iobuf ), align,
					   offset );
		if ( ! data )
			return NULL;
		iobuf = ( data + len );

	} else {

		/* Allocate memory for buffer */
		data = malloc_dma_offset ( len, align, offset );
		if ( ! data )
			return NULL;

		/* Allocate memory for descriptor */
		iobuf = malloc ( sizeof ( *iobuf ) );
		if ( ! iobuf ) {
			free_dma ( data, len );
			return NULL;
		}
	}

	/* Populate descriptor */
	iobuf->head = iobuf->data = iobuf->tail = data;
	iobuf->end = ( data + len );

	return iobuf;
}

/**
 * Allocate I/O buffer
 *
 * @v len	Required length of buffer
 * @ret iobuf	I/O buffer, or NULL if none available
 *
 * The I/O buffer will be physically aligned on its own size (rounded
 * up to the nearest power of two).
 */
struct io_buffer * alloc_iob ( size_t len ) {

	/* Pad to minimum length */
	if ( len < IOB_ZLEN )
		len = IOB_ZLEN;

	/* Align buffer on its own size to avoid potential problems
	 * with boundary-crossing DMA.
	 */
	return alloc_iob_raw ( len, len, 0 );
}

/**
 * Free I/O buffer
 *
 * @v iobuf	I/O buffer
 */
void free_iob ( struct io_buffer *iobuf ) {
	size_t len;

	/* Allow free_iob(NULL) to be valid */
	if ( ! iobuf )
		return;

	/* Sanity checks */
	assert ( iobuf->head <= iobuf->data );
	assert ( iobuf->data <= iobuf->tail );
	assert ( iobuf->tail <= iobuf->end );

	/* Free buffer */
	len = ( iobuf->end - iobuf->head );
	if ( iobuf->end == iobuf ) {

		/* Descriptor is inline */
		free_dma ( iobuf->head, ( len + sizeof ( *iobuf ) ) );

	} else {

		/* Descriptor is detached */
		free_dma ( iobuf->head, len );
		free ( iobuf );
	}
}

/**
 * Ensure I/O buffer has sufficient headroom
 *
 * @v iobuf	I/O buffer
 * @v len	Required headroom
 *
 * This function currently only checks for the required headroom; it
 * does not reallocate the I/O buffer if required.  If we ever have a
 * code path that requires this functionality, it's a fairly trivial
 * change to make.
 */
int iob_ensure_headroom ( struct io_buffer *iobuf, size_t len ) {

	if ( iob_headroom ( iobuf ) >= len )
		return 0;
	return -ENOBUFS;
}

