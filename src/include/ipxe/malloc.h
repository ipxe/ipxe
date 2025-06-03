#ifndef _IPXE_MALLOC_H
#define _IPXE_MALLOC_H

#include <stdint.h>

/** @file
 *
 * Dynamic memory allocation
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/*
 * Prototypes for the standard functions (malloc() et al) are in
 * stdlib.h.  Include <ipxe/malloc.h> only if you need the
 * non-standard functions, such as malloc_phys().
 *
 */
#include <stdlib.h>
#include <ipxe/list.h>
#include <ipxe/tables.h>
#include <valgrind/memcheck.h>

/**
 * Address for zero-length memory blocks
 *
 * @c malloc(0) or @c realloc(ptr,0) will return the special value @c
 * NOWHERE.  Calling @c free(NOWHERE) will have no effect.
 *
 * This is consistent with the ANSI C standards, which state that
 * "either NULL or a pointer suitable to be passed to free()" must be
 * returned in these cases.  Using a special non-NULL value means that
 * the caller can take a NULL return value to indicate failure,
 * without first having to check for a requested size of zero.
 *
 * Code outside of the memory allocators themselves does not ever need
 * to refer to the actual value of @c NOWHERE; this is an internal
 * definition.
 */
#define NOWHERE ( ( void * ) ~( ( intptr_t ) 0 ) )

/** A heap */
struct heap {
	/** List of free memory blocks */
	struct list_head blocks;

	/** Alignment for free memory blocks */
	size_t align;
	/** Alignment for size-tracked allocations */
	size_t ptr_align;

	/** Total amount of free memory */
	size_t freemem;
	/** Total amount of used memory */
	size_t usedmem;
	/** Maximum amount of used memory */
	size_t maxusedmem;

	/**
	 * Attempt to grow heap (optional)
	 *
	 * @v size		Failed allocation size
	 * @ret grown		Heap has grown: retry allocations
	 */
	unsigned int ( * grow ) ( size_t size );
	/**
	 * Allow heap to shrink (optional)
	 *
	 * @v ptr		Start of free block
	 * @v size		Size of free block
	 * @ret shrunk		Heap has shrunk: discard block
	 *
	 * Note that the discarded block will be accessed once after
	 * this method returns, in order to clear the free block
	 * metadata.
	 */
	unsigned int ( * shrink ) ( void *ptr, size_t size );
};

extern void * heap_realloc ( struct heap *heap, void *old_ptr,
			     size_t new_size );
extern void heap_dump ( struct heap *heap );
extern void heap_populate ( struct heap *heap, void *start, size_t len );

extern void * __malloc malloc_phys_offset ( size_t size, size_t phys_align,
					    size_t offset );
extern void * __malloc malloc_phys ( size_t size, size_t phys_align );
extern void free_phys ( void *ptr, size_t size );

/** A cache discarder */
struct cache_discarder {
	/**
	 * Discard some cached data
	 *
	 * @ret discarded	Number of cached items discarded
	 */
	unsigned int ( * discard ) ( void );
};

/** Cache discarder table */
#define CACHE_DISCARDERS __table ( struct cache_discarder, "cache_discarders" )

/** Declare a cache discarder */
#define __cache_discarder( cost ) __table_entry ( CACHE_DISCARDERS, cost )

/** @defgroup cache_cost Cache discarder costs
 *
 * @{
 */

#define CACHE_CHEAP	01	/**< Items with a low replacement cost */
#define CACHE_NORMAL	02	/**< Items with a normal replacement cost */
#define CACHE_EXPENSIVE	03	/**< Items with a high replacement cost */

/** @} */

#endif /* _IPXE_MALLOC_H */
