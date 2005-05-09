#ifndef BUFFER_H
#define BUFFER_H

#include "stdint.h"

/*
 * "start" and "end" denote the real boundaries of the buffer.  "fill"
 * denotes the offset to the first free block in the buffer.  (If the
 * buffer is full, "fill" will equal ( end - start ) ).
 *
 */
struct buffer {
	physaddr_t	start;
	physaddr_t	end;
	off_t		fill;
};

/*
 * Free blocks in the buffer start with a "tail byte".  If non-zero,
 * this byte indicates that the free block is the tail of the buffer,
 * i.e. occupies all the remaining space up to the end of the buffer.
 * When the tail byte is non-zero, it indicates that the remainder of
 * the descriptor (the struct buffer_free_block) follows the tail
 * byte.
 *
 * This scheme is necessary because we may end up with a tail that is
 * smaller than a struct buffer_free_block.
 *
 */
struct buffer_free_block {
	char		tail;
	physaddr_t	next_free;
	physaddr_t	end;
} __attribute__ (( packed ));

/* Functions in buffer.c */

extern void init_buffer ( struct buffer *buffer, physaddr_t start,
			  size_t len );
extern int fill_buffer ( struct buffer *buffer, void *data,
			 off_t offset, size_t len );

#endif /* BUFFER_H */
