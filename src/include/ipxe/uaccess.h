#ifndef _IPXE_UACCESS_H
#define _IPXE_UACCESS_H

/**
 * @file
 *
 * Access to external ("user") memory
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <string.h>
#include <ipxe/api.h>
#include <config/ioapi.h>

#ifdef UACCESS_FLAT
#define UACCESS_PREFIX_flat
#else
#define UACCESS_PREFIX_flat __flat_
#endif

/**
 * A pointer to a user buffer
 *
 */
typedef void * userptr_t;

/** Equivalent of NULL for user pointers */
#define UNULL ( ( userptr_t ) 0 )

/**
 * @defgroup uaccess_trivial Trivial user access API implementations
 *
 * User access API implementations that can be used by environments in
 * which virtual addresses allow access to all of memory.
 *
 * @{
 *
 */

/**
 * Convert virtual address to user pointer
 *
 * @v addr		Virtual address
 * @ret userptr		User pointer
 */
static inline __always_inline userptr_t
trivial_virt_to_user ( volatile const void *addr ) {
	return ( ( userptr_t ) addr );
}

/** @} */

/**
 * Calculate static inline user access API function name
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 * @ret _subsys_func	Subsystem API function
 */
#define UACCESS_INLINE( _subsys, _api_func ) \
	SINGLE_API_INLINE ( UACCESS_PREFIX_ ## _subsys, _api_func )

/**
 * Provide an user access API implementation
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 * @v _func		Implementing function
 */
#define PROVIDE_UACCESS( _subsys, _api_func, _func ) \
	PROVIDE_SINGLE_API ( UACCESS_PREFIX_ ## _subsys, _api_func, _func )

/**
 * Provide a static inline user access API implementation
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 */
#define PROVIDE_UACCESS_INLINE( _subsys, _api_func ) \
	PROVIDE_SINGLE_API_INLINE ( UACCESS_PREFIX_ ## _subsys, _api_func )

static inline __always_inline void *
UACCESS_INLINE ( flat, phys_to_virt ) ( physaddr_t phys ) {
	return ( ( void * ) phys );
}

static inline __always_inline physaddr_t
UACCESS_INLINE ( flat, virt_to_phys ) ( volatile const void *virt ) {
	return ( ( physaddr_t ) virt );
}

static inline __always_inline userptr_t
UACCESS_INLINE ( flat, virt_to_user ) ( volatile const void *addr ) {
	return trivial_virt_to_user ( addr );
}

/* Include all architecture-independent user access API headers */
#include <ipxe/linux/linux_uaccess.h>

/* Include all architecture-dependent user access API headers */
#include <bits/uaccess.h>

/**
 * Convert virtual address to user pointer
 *
 * @v addr		Virtual address
 * @ret userptr		User pointer
 */
userptr_t virt_to_user ( volatile const void *addr );

/**
 * Convert virtual address to a physical address
 *
 * @v virt		Virtual address
 * @ret phys		Physical address
 */
physaddr_t __attribute__ (( const ))
virt_to_phys ( volatile const void *virt );

/**
 * Convert physical address to a virtual address
 *
 * @v phys		Physical address
 * @ret virt		Virtual address
 *
 * This operation is not available under all memory models.
 */
void * __attribute__ (( const )) phys_to_virt ( physaddr_t phys );

/**
 * Copy data to user buffer
 *
 * @v dest		Destination
 * @v dest_off		Destination offset
 * @v src		Source
 * @v len		Length
 */
static inline __always_inline void
copy_to_user ( userptr_t dest, off_t dest_off, const void *src, size_t len ) {
	memcpy ( ( dest + dest_off ), src, len );
}

/**
 * Copy data from user buffer
 *
 * @v dest		Destination
 * @v src		Source
 * @v src_off		Source offset
 * @v len		Length
 */
static inline __always_inline void
copy_from_user ( void *dest, userptr_t src, off_t src_off, size_t len ) {
	memcpy ( dest, ( src + src_off ), len );
}

#endif /* _IPXE_UACCESS_H */
