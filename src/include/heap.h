#ifndef HEAP_H
#define HEAP_H

/*
 * Allocate a block with specified (physical) alignment
 *
 * "align" must be a power of 2.
 *
 * Note that "align" affects the alignment of the physical address,
 * not the virtual address.  This is almost certainly what you want.
 *
 */
extern void * emalloc ( size_t size, unsigned int align );

/*
 * Allocate a block, with no particular alignment requirements.
 *
 */
static inline void * malloc ( size_t size ) {
	return emalloc ( size, sizeof ( void * ) );
}

/*
 * Allocate all remaining space on the heap
 *
 */
extern void * emalloc_all ( size_t *size );

/*
 * Free a block.
 *
 * The caller must ensure that the block being freed is the last (most
 * recent) block allocated on the heap, otherwise heap corruption will
 * occur.
 *
 */
extern void efree ( void *ptr );

static inline void free ( void *ptr ) {
	efree ( ptr );
}

/*
 * Free all allocated blocks on the heap
 *
 */
extern void efree_all ( void );

/*
 * Resize a block.
 *
 * The caller must ensure that the block being resized is the last
 * (most recent) block allocated on the heap, otherwise heap
 * corruption will occur.
 *
 */
static inline void * erealloc ( void *ptr, size_t size, unsigned int align ) {
	efree ( ptr );
	return emalloc ( size, align );
}

#endif /* HEAP_H */
