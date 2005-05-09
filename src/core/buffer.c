/*
 * Routines for filling a buffer with data received piecemeal, where
 * the size of the data is not necessarily known in advance.
 *
 * Some protocols do not provide a mechanism for us to know the size
 * of the file before we happen to receive a particular block
 * (e.g. the final block in an MTFTP transfer).  In addition, some
 * protocols (all the multicast protocols plus any TCP-based protocol)
 * can, in theory, provide the data in any order.
 *
 * Rather than requiring each protocol to implement its own equivalent
 * of "dd" to arrange the data into well-sized pieces before handing
 * off to the image loader, we provide these generic buffer functions
 * which assemble a file into a single contiguous block.  The whole
 * block is then passed to the image loader.
 *
 */

#include "stddef.h"
#include "string.h"
#include "io.h"
#include "buffer.h"

/*
 * Initialise a buffer
 *
 */
void init_buffer ( struct buffer *buffer, physaddr_t start, size_t len ) {
	buffer->start = start;
	buffer->end = start + len;
	buffer->first_free = start;

	if ( len ) {
		char tail = 1;
		copy_to_phys ( start, &tail, sizeof ( tail ) );
	}
}

/*
 * Split a free block
 *
 */
static void split_free_block ( struct buffer_free_block *desc,
			       physaddr_t block, physaddr_t split ) {
	/* If split point is before start of block, do nothing */
	if ( split <= block )
		return;

	/* If split point is after end of block, do nothing */
	if ( split >= desc->end )
		return;

	/* Create descriptor for new free block */
	copy_to_phys ( split, &desc->tail, sizeof ( desc->tail ) );
	if ( ! desc->tail )
		copy_to_phys ( split, desc, sizeof ( *desc ) );

	/* Update descriptor for old free block */
	desc->tail = 0;
	desc->next_free = split;
	desc->end = split;
	copy_to_phys ( block, desc, sizeof ( *desc ) );
}

/*
 * Mark a free block as used
 *
 */
static inline void unfree_block ( struct buffer *buffer,
				  struct buffer_free_block *desc,
				  physaddr_t prev_block ) {
	struct buffer_free_block prev_desc;
	
	/* If this is the first block, just update first_free */
	if ( ! prev_block ) {
		buffer->first_free = desc->next_free;
		return;
	}

	/* Get descriptor for previous block (which cannot be a tail block) */
	copy_from_phys ( &prev_desc, prev_block, sizeof ( prev_desc ) );

	/* Modify descriptor for previous block and write it back */
	prev_desc.next_free = desc->next_free;
	copy_to_phys ( prev_block, &prev_desc, sizeof ( prev_desc ) );
}

/*
 * Write data into a buffer
 *
 * It is the caller's responsibility to ensure that the boundaries
 * between data blocks are more than sizeof(struct buffer_free_block)
 * apart.  If this condition is not satisfied, data corruption will
 * occur.
 *
 * Returns the offset to the first gap in the buffer.  (When the
 * buffer is full, returns the offset to the byte past the end of the
 * buffer.)
 *
 */
off_t fill_buffer ( struct buffer *buffer, void *data,
		    off_t offset, size_t len ) {
	struct buffer_free_block desc;
	physaddr_t block, prev_block;
	physaddr_t data_start, data_end;

	/* Calculate start and end addresses of data */
	data_start = buffer->start + offset;
	data_end = data_start + len;

	/* Iterate through the buffer's free blocks */
	prev_block = 0;
	block = buffer->first_free;
	while ( block < buffer->end ) {
		/* Read block descriptor */
		desc.next_free = buffer->end;
		desc.end = buffer->end;
		copy_from_phys ( &desc.tail, block, sizeof ( desc.tail ) );
		if ( ! desc.tail )
			copy_from_phys ( &desc, block, sizeof ( desc ) );

		/* Split block at data start and end markers */
		split_free_block ( &desc, block, data_start );
		split_free_block ( &desc, block, data_end );

		/* Block is now either completely contained by or
		 * completely outside the data area
		 */
		if ( ( block >= data_start ) && ( block <= data_end ) ) {
			/* Block is within the data area */
			unfree_block ( buffer, &desc, prev_block );
			copy_to_phys ( block, data + ( block - data_start ),
				       desc.end - block );
		} else {
			/* Block is outside the data area */
			prev_block = block;
		}

		/* Move to next free block */
		block = desc.next_free;
	}

	return ( buffer->first_free - buffer->start );
}
