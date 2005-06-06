#ifndef BUFFER_H
#define BUFFER_H

#include "compiler.h" /* for doxygen */
#include "stdint.h"

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
 * For a description of the internal operation, see buffer.c.
 *
 */

/**
 * A buffer
 *
 * #start and #end denote the real boundaries of the buffer, and are
 * physical addresses.  #fill denotes the offset to the first free
 * block in the buffer.  (If the buffer is full, #fill will equal
 * #end-#start.)
 *
 */
struct buffer {
	physaddr_t	start;		/**< Start of buffer in memory */
	physaddr_t	end;		/**< End of buffer in memory */
	off_t		fill;		/**< Offset to first gap in buffer */
};

/**
 * A free block descriptor.
 *
 * See buffer.c for a full description of the fields.
 *
 */
struct buffer_free_block {
	char		tail;		/**< Tail byte marker */
	char		reserved[3];	/**< Padding */
	physaddr_t	start;		/**< Address of this free block */
	physaddr_t	next;		/**< Address of next free block */
	physaddr_t	end;		/**< End of this block */
} __attribute__ (( packed ));

/* Functions in buffer.c */

extern void init_buffer ( struct buffer *buffer );
extern int fill_buffer ( struct buffer *buffer, const void *data,
			 off_t offset, size_t len );

#endif /* BUFFER_H */
