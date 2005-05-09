#ifndef BUFFER_H
#define BUFFER_H

#include "stdint.h"

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

struct buffer {
	physaddr_t	start;
	physaddr_t	end;
       	physaddr_t	first_free;
};

/* Functions in buffer.c */

extern void init_buffer ( struct buffer *buffer, physaddr_t start,
			  size_t len );
extern off_t fill_buffer ( struct buffer *buffer, void *data,
			   off_t offset, size_t len );

#endif /* BUFFER_H */
