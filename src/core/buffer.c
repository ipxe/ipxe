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

#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <gpxe/uaccess.h>
#include <gpxe/buffer.h>

/** @file
 *
 * Buffer internals.
 *
 * A buffer consists of a single, contiguous area of memory, some of
 * which is "filled" and the remainder of which is "free".  The
 * "filled" and "free" spaces are not necessarily contiguous.
 *
 * At the start of a buffer's life, it consists of a single free
 * space.  As data is added to the buffer via fill_buffer(), this free
 * space decreases and can become fragmented.
 *
 * Each free block within a buffer (except the last) starts with a @c
 * struct @c buffer_free_block.  This describes the size of the free
 * block, and the offset to the next free block.
 *
 * We cannot simply start every free block (including the last) with a
 * descriptor, because it is conceivable that we will, at some point,
 * encounter a situation in which the final free block of a buffer is
 * too small to contain a descriptor.  Consider a protocol with a
 * blocksize of 512 downloading a 1025-byte file into a 1025-byte
 * buffer.  Suppose that the first two blocks are received; we have
 * now filled 1024 of the 1025 bytes in the buffer, and our only free
 * block consists of the 1025th byte.
 * 
 * Note that the rather convoluted way of manipulating the buffer
 * descriptors (using copy_{to,from}_user rather than straightforward
 * pointers) is needed to cope with operation as a PXE stack, when we
 * may be running in real mode or 16-bit protected mode, and therefore
 * cannot directly access arbitrary areas of memory using simple
 * pointers.
 *
 */

/**
 * A free block descriptor
 *
 * This is the data structure that is found at the start of a free
 * block within a data buffer.
 */
struct buffer_free_block {
	/** Starting offset of the free block */
	size_t start;
	/** Ending offset of the free block */
	size_t end;
	/** Offset of next free block */
	size_t next;
};

/**
 * Get next free block within the buffer
 *
 * @v buffer		Data buffer
 * @v block		Previous free block descriptor
 * @ret block		Next free block descriptor
 * @ret rc		Return status code
 *
 * Set @c block->next=buffer->fill before first call to
 * get_next_free_block().
 */
static int get_next_free_block ( struct buffer *buffer,
				 struct buffer_free_block *block ) {

	/* Check for end of buffer */
	if ( block->next >= buffer->len )
		return -ENOENT;

	/* Move to next block */
	block->start = block->next;
	if ( block->start >= buffer->free ) {
		/* Final block; no in-band descriptor */
		block->next = block->end = buffer->len;
	} else {
		/* Retrieve block descriptor */
		copy_from_user ( block, buffer->addr, block->start,
				 sizeof ( *block ) );
	}

	return 0;
}

/**
 * Write free block descriptor back to buffer
 *
 * @v buffer		Data buffer
 * @v block		Free block descriptor
 */
static void store_free_block ( struct buffer *buffer,
			       struct buffer_free_block *block ) {
	size_t free_block_size = ( block->end - block->start );

	assert ( free_block_size >= sizeof ( *block ) );
	copy_to_user ( buffer->addr, block->start, block, sizeof ( *block ) );
}

/**
 * Write data into a buffer
 *
 * @v buffer		Data buffer
 * @v data		Data to be written
 * @v offset		Offset within the buffer at which to write the data
 * @v len		Length of data to be written
 * @ret rc		Return status code
 *
 * Writes a block of data into the buffer.  The block need not be
 * aligned to any particular boundary, or be of any particular size,
 * and it may overlap blocks already in the buffer (i.e. duplicate
 * calls to fill_buffer() are explicitly permitted).
 *
 * @c buffer->fill will be updated to indicate the fill level of the
 * buffer, i.e. the offset to the first gap within the buffer.  If the
 * filesize is known (e.g. as with the SLAM protocol), you can test
 * for end-of-file by checking for @c buffer->fill==filesize.  If the
 * filesize is not known, but there is a well-defined end-of-file test
 * (e.g. as with the TFTP protocol), you can read @c buffer->fill to
 * determine the final filesize.  If blocks are known to be delivered
 * in a strictly sequential order with no packet loss or duplication,
 * then you can pass in @c offset==buffer->fill.
 *
 * @b NOTE: It is the caller's responsibility to ensure that the
 * boundaries between data blocks are more than @c sizeof(struct @c
 * buffer_free_block) apart.  If this condition is not satisfied, data
 * corruption will occur.
 *
 * In practice this is not a problem.  Callers of fill_buffer() will
 * be download protocols such as TFTP, and very few protocols have a
 * block size smaller than @c sizeof(struct @c buffer_free_block).
 *
 */
int fill_buffer ( struct buffer *buffer, const void *data,
		  size_t offset, size_t len ) {
	struct buffer_free_block block, before, after;
	size_t data_start = offset;
	size_t data_end = ( data_start + len );
	int rc;

	DBGC2 ( buffer, "BUFFER %p [%lx,%lx) filling portion [%lx,%lx)\n",
		buffer, user_to_phys ( buffer->addr, 0 ),
		user_to_phys ( buffer->addr, buffer->len ),
		user_to_phys ( buffer->addr, data_start ),
		user_to_phys ( buffer->addr, data_end ) );

	/* Check that block fits within buffer, expand if necessary */
	if ( data_end > buffer->len ) {
		if ( ( rc = expand_buffer ( buffer, data_end ) ) != 0 )
			return rc;
		assert ( buffer->len >= data_end );
	}

	/* Find 'before' and 'after' blocks, if any */
	before.start = before.end = 0;
	after.start = after.end = buffer->len;
	block.next = buffer->fill;
	while ( get_next_free_block ( buffer, &block ) == 0 ) {
		if ( ( block.start < data_start ) &&
		     ( block.start >= before.start ) )
			memcpy ( &before, &block, sizeof ( before ) );
		if ( ( block.end > data_end ) &&
		     ( block.end <= after.end ) )
			memcpy ( &after, &block, sizeof ( after ) );
	}

	/* Truncate 'before' and 'after' blocks around data. */
	if ( data_start < before.end )
		before.end = data_start;
	if ( data_end > after.start )
		after.start = data_end;

	/* Link 'after' block to 'before' block */
	before.next = after.start;

	DBGC2 ( buffer, "BUFFER %p split before [%lx,%lx) after [%lx,%lx)\n",
		buffer, user_to_phys ( buffer->addr, before.start ),
		user_to_phys ( buffer->addr, before.end ),
		user_to_phys ( buffer->addr, after.start ),
		user_to_phys ( buffer->addr, after.end ) );

	/* Write back 'before' block, if any */
	if ( before.end == 0 ) {
		/* No 'before' block: update buffer->fill */
		buffer->fill = after.start;
		DBGC2 ( buffer, "BUFFER %p full up to %lx\n", buffer,
			user_to_phys ( buffer->addr, buffer->fill ) );
	} else {
		/* Write back 'before' block */
		store_free_block ( buffer, &before );
	}

	/* Write back 'after' block */
	if ( after.end == buffer->len ) {
		/* 'After' block is the final block: update buffer->free */
		buffer->free = after.start;
		DBGC2 ( buffer, "BUFFER %p free from %lx onwards\n", buffer,
			user_to_phys ( buffer->addr, buffer->free ) );
	} else {
		/* Write back 'after' block */
		store_free_block ( buffer, &after );
	}

	/* Copy data into buffer */
	copy_to_user ( buffer->addr, data_start, data, len );

	return 0;
}

/** Expand data buffer
 *
 * @v buffer		Data buffer
 * @v new_len		New length
 * @ret rc		Return status code
 *
 * Expand the data buffer to accommodate more data.  Some buffers may
 * not support being expanded.
 */
int expand_buffer ( struct buffer *buffer, size_t new_len ) {
	int rc;

	if ( new_len <= buffer->len )
		return 0;

	DBGC ( buffer, "BUFFER %p attempting to expand from length %zx to "
	       "length %zx\n", buffer, buffer->len, new_len );

	if ( ! buffer->expand ) {
		DBGC ( buffer, "BUFFER %p is not expandable\n", buffer );
		return -ENOBUFS;
	}

	if ( ( rc = buffer->expand ( buffer, new_len ) ) != 0 ) {
		DBGC ( buffer, "BUFFER %p could not expand: %s\n",
		       buffer, strerror ( rc ) );
		return rc;
	}

	DBGC ( buffer, "BUFFER %p expanded to [%lx,%lx)\n", buffer,
	       user_to_phys ( buffer->addr, 0 ),
	       user_to_phys ( buffer->addr, buffer->len ) );

	return 0;
}
