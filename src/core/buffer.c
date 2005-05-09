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
#include "buffer.h"

/*
 * Split a free block at the specified address, to produce two
 * consecutive free blocks.  If the address is not within the free
 * block, do nothing and return success.  If one of the resulting free
 * blocks would be too small to contain the free block descriptor,
 * return failure.
 *
 */
static int split_free_block ( struct buffer_free_block *block, void *split ) {
	struct buffer_free_block *new_block = split;

	if ( ( split <= ( void * ) block ) || ( split >= block->end ) ) {
		/* Split is outside block; nothing to do */
		return 1;
	}
	
	if ( ( ( block + 1 ) > new_block ) ||
	     ( ( ( void * ) ( new_block + 1 ) ) > block->end ) ) {
		/* Split block would be too small; fail */
		return 0;
	}

	/* Create new block, link into free list */
	new_block->next		= block->next;
	new_block->next->prev	= new_block;
	new_block->prev		= block->prev;
	new_block->end 		= block->end;
	block->next		= new_block;
	block->end		= new_block;
	return 1;
}

/*
 * Remove a block from the free list.
 *
 * Note that this leaves block->next intact.
 *
 */
static inline void unfree_block ( struct buffer_free_block *block ) {
	block->prev->next = block->next;
	block->next->prev = block->prev;
}

/*
 * Mark a stretch of memory within a buffer as allocated.
 *
 */
static inline int mark_allocated ( struct buffer *buffer,
				   void *start, void *end ) {
	struct buffer_free_block *block = buffer->free_blocks.next;

	while ( block != &buffer->free_blocks ) {
		if ( ! ( split_free_block ( block, start ) &&
			 split_free_block ( block, end ) ) ) {
			/* Block split failure; fail */
			return 0;
		}
		/* At this point, block can be entirely contained
		 * within [start,end), but it can't overlap.
		 */
		if ( ( ( ( void * ) block ) >= start ) &&
		     ( ( ( void * ) block ) < end ) ) {
			unfree_block ( block );
		}
		block = block->next;
	}

	return 1;
}

/*
 * Place data into a buffer
 *
 */
int fill_buffer ( struct buffer *buffer, void *data,
		  off_t offset, size_t len ) {
	void *start = buffer->start + offset;
	void *end = start + len;

	if ( ! mark_allocated ( buffer, start, end ) ) {
		/* Allocation failure; fail */
		return 0;
	}
	memcpy ( start, data, len );
	return 1;
}

/*
 * Initialise a buffer
 *
 */
static void init_buffer ( struct buffer *buffer, void *start, size_t len ) {
	struct buffer_free_block *block;

	block = start;
	block->next = &buffer->free_blocks;
	block->prev = &buffer->free_blocks;
	block->end = start + len;

	buffer->free_blocks.next = block;
	buffer->free_blocks.prev = block;
	buffer->start = start;
	buffer->end = start + len;
}

/*
 * Move a buffer
 *
 */
