/** @file
 *
 * Buffers for loading files.
 *
 * This file provides routines for filling a buffer with data received
 * piecemeal, where the size of the data is not necessarily known in
 * advance.
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
 * Example usage:
 *
 * @code
 *
 *   struct buffer my_buffer;
 *   void *data;
 *   off_t offset;
 *   size_t len;
 *   
 *   // We have an area of memory [buf_start,buf_end) into which we want
 *   // to load a file, where buf_start and buf_end are physical addresses.
 *   buffer->start = buf_start;
 *   buffer->end = buf_end;
 *   init_buffer ( &buffer );
 *   ...
 *   while ( get_file_block ( ... ) ) {
 *     // Downloaded block is stored in [data,data+len), and represents 
 *     // the portion of the file at offsets [offset,offset+len)
 *     if ( ! fill_buffer ( &buffer, data, offset, len ) ) {
 *       // An error occurred
 *       return 0;
 *     }
 *     ...
 *   }
 *   ...
 *   // The whole file is now present at [buf_start,buf_start+filesize),
 *   // where buf_start is a physical address.  The struct buffer can simply
 *   // be discarded; there is no done_buffer() call.
 *
 * @endcode
 *
 * For a description of the internal operation, see \ref buffer_int.
 *
 */

/** @page buffer_int Buffer internals
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
#include "buffer.h"

/**
 * Initialise a buffer.
 *
 * @v buffer		The buffer to be initialised
 * @ret None
 * @err None
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
 * Split a free block.
 *
 * @v desc		A descriptor for the free block
 * @v block		Start address of the block
 * @v split		Address at which to split the block
 * @ret None
 * @err None
 *
 * Split a free block into two separate free blocks.  If the split
 * point lies outside the block, no action is taken; this is not an
 * error.
 *
 * @b NOTE: It is the reponsibility of the caller to ensure that there
 * is enough room in each of the two portions for a free block
 * descriptor (a @c struct @c buffer_free_block, except in the case of
 * a tail block which requires only a one byte descriptor).  If the
 * caller fails to do this, data corruption will occur.
 *
 * In practice, this means that the granularity at which blocks are
 * split must be at least @c sizeof(struct @c buffer_free_block).
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

	DBG ( "BUFFER splitting [%x,%x) -> [%x,%x) [%x,%x)\n",
	      block, desc->end, block, split, split, desc->end );

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

/**
 * Mark a free block as used.
 *
 * @v buffer		The buffer containing the block
 * @v desc		A descriptor for the free block
 * @v prev_block	Address of the previous block
 * @ret None
 * @err None
 *
 * Marks a free block as used, i.e. removes it from the free list.
 *
 */
static inline void unfree_block ( struct buffer *buffer,
				  struct buffer_free_block *desc,
				  physaddr_t prev_block ) {
	struct buffer_free_block prev_desc;
	
	/* If this is the first block, just update buffer->fill */
	if ( ! prev_block ) {
		DBG ( "BUFFER marking [%x,%x) as used\n",
		      buffer->start + buffer->fill, desc->end );
		buffer->fill = desc->next_free - buffer->start;
		return;
	}

	/* Get descriptor for previous block (which cannot be a tail block) */
	copy_from_phys ( &prev_desc, prev_block, sizeof ( prev_desc ) );

	DBG ( "BUFFER marking [%x,%x) as used\n",
	      prev_desc.next_free, desc->end );

	/* Modify descriptor for previous block and write it back */
	prev_desc.next_free = desc->next_free;
	copy_to_phys ( prev_block, &prev_desc, sizeof ( prev_desc ) );
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
 * corruption will occur.  (See split_free_block() for details.)
 *
 * In practice this is not a problem.  Callers of fill_buffer() will
 * be download protocols such as TFTP, and very few protocols have a
 * block size smaller than @c sizeof(struct @c buffer_free_block).
 *
 */
int fill_buffer ( struct buffer *buffer, const void *data,
		  off_t offset, size_t len ) {
	struct buffer_free_block desc;
	physaddr_t block, prev_block;
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

	/* Iterate through the buffer's free blocks */
	prev_block = 0;
	block = buffer->start + buffer->fill;
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
		if ( ( block >= data_start ) && ( block < data_end ) ) {
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

	DBG ( "BUFFER [%x,%x) full up to %x\n",
	      buffer->start, buffer->end, buffer->start + buffer->fill );

	return 1;
}
