#ifndef _GPXE_MALLOC_H
#define _GPXE_MALLOC_H

#include <stdint.h>

/** @file
 *
 * Memory allocation
 *
 */

extern void * gmalloc ( size_t size );
extern void gfree ( void *ptr, size_t size );
extern void gmpopulate ( void *start, size_t len );

/**
 * Allocate cleared memory
 *
 * @v size		Requested size
 * @ret ptr		Allocated memory
 *
 * Allocate memory as per gmalloc(), and zero it.
 *
 * Note that gmalloc() and gcalloc() are identical, in the interests
 * of reducing code size.  Callers should not, however, rely on
 * gmalloc() clearing memory, since this behaviour may change in
 * future.
 */
static inline void * gcalloc ( size_t size ) {
	return gmalloc ( size );
}

/* Debug function; not compiled in by default */
void gdumpfree ( void );

#endif /* _GPXE_MALLOC_H */
