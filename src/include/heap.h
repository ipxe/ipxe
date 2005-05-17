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
extern void * erealloc ( void *ptr, size_t size );

/*
 * Allocate, free, and resize blocks without caring about alignment
 *
 */
static inline void * malloc ( size_t size ) {
	return emalloc ( size, sizeof ( void * ) );
}

static inline void free ( void *ptr ) {
	efree ( ptr );
}

static inline void * realloc ( void *ptr, size_t size ) {
	return erealloc ( ptr, size );
}

/*
 * Legacy API calls
 *
 */
static inline void * allot ( size_t size ) {
	return emalloc ( size, sizeof ( void * ) );
}

static inline void forget ( void *ptr ) {
	efree ( ptr );
}

static inline void * allot2 ( size_t size, uint32_t mask ) {
	return emalloc ( size, mask + 1 );
}

static inline void forget2 ( void *ptr ) {
	efree ( ptr );
}

/*
 * Heap markers.  osloader.c and other code may wish to know the heap
 * location, without necessarily wanting to drag in heap.o.  We
 * therefore declare these as shared (i.e. common) symbols.
 *
 */
physaddr_t heap_ptr __asm__ ( "_shared_heap_ptr" );
physaddr_t heap_end __asm__ ( "_shared_heap_end" );

#endif /* HEAP_H */
