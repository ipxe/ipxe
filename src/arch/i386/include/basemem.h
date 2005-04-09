#ifndef BASEMEM_H
#define BASEMEM_H

#ifdef ASSEMBLY

/* Must match sizeof(struct free_base_memory_header) */
#define FREE_BASEMEM_HEADER_SIZE 8

#else /* ASSEMBLY */

#include "stdint.h"

/* Structures that we use to represent a free block of base memory */

#define FREE_BLOCK_MAGIC ( ('!'<<0) + ('F'<<8) + ('R'<<16) + ('E'<<24) )
struct free_base_memory_header {
	uint32_t	magic;
	uint32_t	size_kb;
};

union free_base_memory_block {
	struct free_base_memory_header;
	char bytes[1024];
};

/* Function prototypes */
extern uint32_t get_free_base_memory ( void );
extern void * alloc_base_memory ( size_t size );
extern void free_base_memory ( void *ptr, size_t size );

#endif /* ASSEMBLY */

#endif /* BASEMEM_H */
