#ifndef BUFFER_H
#define BUFFER_H

#include "stdint.h"

/* @file */

/**
 * A buffer
 *
 * @c start and @c end denote the real boundaries of the buffer, and
 * are physical addresses.  @c fill denotes the offset to the first
 * free block in the buffer.  (If the buffer is full, @c fill will
 * equal @c end-start.)
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
 * See \ref buffer_int for a full description of the fields.
 *
 */
struct buffer_free_block {
	char		tail;		/**< Tail byte marker */
	physaddr_t	next_free;	/**< Address of next free block */
	physaddr_t	end;		/**< End of this block */
} __attribute__ (( packed ));

/* Functions in buffer.c */

extern void init_buffer ( struct buffer *buffer );
extern int fill_buffer ( struct buffer *buffer, const void *data,
			 off_t offset, size_t len );

#endif /* BUFFER_H */
