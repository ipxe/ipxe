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
#include <ipxe/api.h>
#include <config/ioapi.h>

#ifdef UACCESS_FLAT
#define UACCESS_PREFIX_flat
#else
#define UACCESS_PREFIX_flat __flat_
#endif

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

/* Include all architecture-independent user access API headers */
#include <ipxe/virt_offset.h>
#include <ipxe/linux/linux_uaccess.h>

/* Include all architecture-dependent user access API headers */
#include <bits/uaccess.h>

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

#endif /* _IPXE_UACCESS_H */
