#ifndef _GPXE_BUFFER_H
#define _GPXE_BUFFER_H

#include <stdint.h>
#include <gpxe/uaccess.h>

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
 * protocols (e.g. the multicast protocols) can, in theory, provide
 * the data in any order.
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
 *   // We have an area of memory [buf_start,buf_start+len) into which to
 *   // load a file, where buf_start is a userptr_t.
 *   memset ( &buffer, 0, sizeof ( buffer ) );
 *   buffer->start = buf_start;
 *   buffer->len = len;
 *   ...
 *   while ( get_file_block ( ... ) ) {
 *     // Downloaded block is stored in [data,data+len), and represents 
 *     // the portion of the file at offsets [offset,offset+len)
 *     if ( fill_buffer ( &buffer, data, offset, len ) != 0 ) {
 *       // An error occurred
 *     }
 *     ...
 *   }
 *   ...
 *   // The whole file is now present at [buf_start,buf_start+filesize),
 *   // where buf_start is a userptr_t.  The struct buffer can simply
 *   // be discarded.
 *
 * @endcode
 *
 */

/**
 * A data buffer
 *
 * A buffer looks something like this:
 *
 * @code
 *
 *     XXXXXXXXXXXXXXXXX.........XXX..........XXXXXXX........XXXXXX.........
 *
 *     ^
 *     |
 *   start
 *
 *     <----- fill ---->
 *
 *     <------------------------ free ---------------------------->
 *
 *     <------------------------------ len -------------------------------->
 *
 * @endcode
 *
 * #start and #len denote the real boundaries of the buffer.  #fill
 * denotes the offset to the first free block in the buffer.  (If the
 * buffer is full, #fill, #free and #len will all be equal.)
 *
 */
struct buffer {
	/** Start of buffer */
	userptr_t addr;
	/** Total length of buffer */
	size_t len;
	/** Offset to first free block within buffer */
	size_t fill;
	/** Offset to last free block within buffer */
	size_t free;
	/** Expand data buffer
	 *
	 * @v buffer		Data buffer
	 * @v new_len		New length
	 * @ret rc		Return status code
	 *
	 * Expand the data buffer to accommodate more data.  This
	 * method is optional; if it is @c NULL then the buffer will
	 * not be expandable.
	 */
	int ( * expand ) ( struct buffer *buffer, size_t new_len );
};

extern int fill_buffer ( struct buffer *buffer, const void *data,
			 size_t offset, size_t len );

#endif /* _GPXE_BUFFER_H */
