#ifndef STDLIB_H
#define STDLIB_H

extern unsigned long strtoul ( const char *p, char **endp, int base );
extern void * realloc ( void *old_ptr, size_t new_size );
extern void * malloc ( size_t size );
extern void free ( void *ptr );

/**
 * Allocate cleared memory
 *
 * @v nmemb		Number of members
 * @v size		Size of each member
 * @ret ptr		Allocated memory
 *
 * Allocate memory as per malloc(), and zero it.
 *
 * Note that malloc() and calloc() are identical, in the interests of
 * reducing code size.  Callers should not, however, rely on malloc()
 * clearing memory, since this behaviour may change in future.
 */
static inline void * calloc ( size_t nmemb, size_t size ) {
	return malloc ( nmemb * size );
}

#endif /* STDLIB_H */
