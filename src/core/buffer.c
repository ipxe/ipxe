
/** @file
 *
 * Buffer internals.
 *
 * A buffer consists of a single, contiguous area of memory, some of
 * which is "filled" and the remainder of which is "free".  The
 * "filled" and "free" spaces are not necessarily contiguous.
 *
 * When a buffer is initialised via init_buffer(), it consists of a
 * single free space.  As data is added to the buffer via
 * fill_buffer(), this free space decreases and can become fragmented.
 *
 * Each free block within a buffer starts with a "tail byte".  If the
 * tail byte is non-zero, this indicates that the free block is the
 * tail of the buffer, i.e. occupies all the remaining space up to the
 * end of the buffer.  When the tail byte is non-zero, it indicates
 * that a descriptor (a @c struct @c buffer_free_block) follows the
 * tail byte.  The descriptor describes the size of the free block and
 * the address of the next free block.
 *
 * We cannot simply always start a free block with a descriptor,
 * because it is conceivable that we will, at some point, encounter a
 * situation in which the final free block of a buffer is too small to
 * contain a descriptor.  Consider a protocol with a blocksize of 512
 * downloading a 1025-byte file into a 1025-byte buffer.  Suppose that
 * the first two blocks are received; we have now filled 1024 of the
 * 1025 bytes in the buffer, and our only free block consists of the
 * 1025th byte.  Using a "tail byte" solves this problem.
 *
 * 
 * Note that the rather convoluted way of manipulating the buffer
 * descriptors (using copy_{to,from}_phys rather than straightforward
 * pointers) is needed to cope with operation as a PXE stack, when we
 * may be running in real mode or 16-bit protected mode, and therefore
 * cannot directly access arbitrary areas of memory using simple
 * pointers.
 *
 */

#include "stddef.h"
#include "string.h"
#include "io.h"
#include "errno.h"
#include "buffer.h"

/**
 * Initialise a buffer.
 *
 * @v buffer		The buffer to be initialised
 * @ret None		-
 * @err None		-
 *
 * Set @c buffer->start and @c buffer->end before calling init_buffer().
 * init_buffer() will initialise the buffer to the state of being
 * empty.
 *
 */
void init_buffer ( struct buffer *buffer ) {
	char tail = 1;

	buffer->fill = 0;
	if ( buffer->end != buffer->start )
		copy_to_phys ( buffer->start, &tail, sizeof ( tail ) );

	DBG ( "BUFFER [%x,%x) initialised\n", buffer->start, buffer->end );
}

/**
 * Move to the next block in the free list
 *
 * @v block		The current free block
 * @v buffer		The buffer
 * @ret True		Successfully moved to the next free block
 * @ret False		There are no more free blocks
 * @ret block		The next free block
 * @err None		-
 *
 * Move to the next block in the free block list, filling in @c block
 * with the descriptor for this next block.  If the next block is the
 * tail block, @c block will be filled with the values calculated for
 * the tail block, otherwise the descriptor will be read from the free
 * block itself.
 *
 * If there are no more free blocks, next_free_block() returns False
 * and leaves @c block with invalid contents.
 *
 * Set <tt> block->next = buffer->start + buffer->fill </tt> for the
 * first call to next_free_block().
 */
static inline int next_free_block ( struct buffer_free_block *block,
				    struct buffer *buffer ) {
	/* Move to next block */
	block->start = block->next;

	/* If at end of buffer, return 0 */
	if ( block->start >= buffer->end )
		return 0;

	/* Set up ->next and ->end as for a tail block */
	block->next = block->end = buffer->end;

	/* Read tail marker from block */
	copy_from_phys ( &block->tail, block->start, sizeof ( block->tail ) );

	/* If not a tail block, read whole block descriptor from block */
	if ( ! block->tail ) {
		copy_from_phys ( block, block->start, sizeof ( *block ) );
	}

	return 1;
}

/**
 * Store a free block descriptor
 *
 * @v block		The free block descriptor to store
 * @ret None		-
 * @err None		-
 *
 * Writes a free block descriptor back to a free block.  If the block
 * is a tail block, only the tail marker will be written, otherwise
 * the whole block descriptor will be written.
 */
static inline void store_free_block ( struct buffer_free_block *block ) {
	copy_to_phys ( block->start, block,
		       ( block->tail ?
			 sizeof ( block->tail ) : sizeof ( *block ) ) );
}

/**
 * Write data into a buffer.
 *
 * @v buffer		The buffer into which to write the data
 * @v data		The data to be written
 * @v offset		Offset within the buffer at which to write the data
 * @v len		Length of data to be written
 * @ret True		Data was successfully written
 * @ret False		Data was not written
 * @err ENOMEM		Buffer is too small to contain the data
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
		  off_t offset, size_t len ) {
	struct buffer_free_block block, before, after;
	physaddr_t data_start, data_end;

	/* Calculate start and end addresses of data */
	data_start = buffer->start + offset;
	data_end = data_start + len;
	DBG ( "BUFFER [%x,%x) writing portion [%x,%x)\n",
	      buffer->start, buffer->end, data_start, data_end );

	/* Check buffer bounds */
	if ( data_end > buffer->end ) {
		DBG ( "BUFFER [%x,%x) too small for data!\n",
		      buffer->start, buffer->end );
		errno = ENOMEM;
		return 0;
	}

	/* Find 'before' and 'after' blocks, if any */
	before.start = before.end = 0;
	after.start = after.end = buffer->end;
	block.next = buffer->start + buffer->fill;
	while ( next_free_block ( &block, buffer ) ) {
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

	/* Write back 'before' block, if any */
	if ( before.start ) {
		before.tail = 0;
		ASSERT ( ( before.end - before.start ) >=
			 sizeof ( struct buffer_free_block ) );
		store_free_block ( &before );
	} else {
		buffer->fill = before.next - buffer->start;
	}

	/* Write back 'after' block, if any */
	if ( after.start < buffer->end ) {
		ASSERT ( after.tail ||
			 ( ( after.end - after.start ) >=
			   sizeof ( struct buffer_free_block ) ) );
		store_free_block ( &after );
	}
	
	DBG ( "BUFFER [%x,%x) before [%x,%x) after [%x,%x)\n",
	      buffer->start, buffer->end, before.start, before.end,
	      after.start, after.end );
	
	/* Copy data into buffer */
	copy_to_phys ( data_start, data, len );

	DBG ( "BUFFER [%x,%x) full up to %x\n",
	      buffer->start, buffer->end, buffer->start + buffer->fill );

	return 1;
}
