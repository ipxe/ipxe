#ifndef _IPXE_UMALLOC_H
#define _IPXE_UMALLOC_H

/**
 * @file
 *
 * User memory allocation
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stddef.h>
#include <ipxe/api.h>
#include <ipxe/malloc.h>
#include <config/umalloc.h>

/**
 * Provide a user memory allocation API implementation
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 * @v _func		Implementing function
 */
#define PROVIDE_UMALLOC( _subsys, _api_func, _func ) \
	PROVIDE_SINGLE_API ( UMALLOC_PREFIX_ ## _subsys, _api_func, _func )

/* Include all architecture-independent I/O API headers */
#include <ipxe/uheap.h>
#include <ipxe/efi/efi_umalloc.h>
#include <ipxe/linux/linux_umalloc.h>

/* Include all architecture-dependent I/O API headers */
#include <bits/umalloc.h>

/**
 * Reallocate external memory
 *
 * @v old_ptr		Memory previously allocated by umalloc(), or NULL
 * @v new_size		Requested size
 * @ret new_ptr		Allocated memory, or NULL
 *
 * Calling realloc() with a new size of zero is a valid way to free a
 * memory block.
 */
void * urealloc ( void *ptr, size_t new_size );

/**
 * Allocate external memory
 *
 * @v size		Requested size
 * @ret ptr		Memory, or NULL
 *
 * Memory is guaranteed to be aligned to a page boundary.
 */
static inline __always_inline void * umalloc ( size_t size ) {
	return urealloc ( NULL, size );
}

/**
 * Free external memory
 *
 * @v ptr		Memory allocated by umalloc(), or NULL
 *
 * If @c ptr is NULL, no action is taken.
 */
static inline __always_inline void ufree ( void *ptr ) {
	urealloc ( ptr, 0 );
}

#endif /* _IPXE_UMALLOC_H */
