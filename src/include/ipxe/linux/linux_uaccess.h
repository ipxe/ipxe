#ifndef _IPXE_LINUX_UACCESS_H
#define _IPXE_LINUX_UACCESS_H

/** @file
 *
 * iPXE user access API for Linux
 *
 * We run with no distinction between internal and external addresses,
 * so can use trivial_virt_to_user() et al.
 *
 * We have no concept of the underlying physical addresses, since
 * these are not exposed to userspace.  We provide a stub
 * implementation of virt_to_phys() since this is required by
 * alloc_memblock().  We provide a matching stub implementation of
 * phys_to_virt().
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef UACCESS_LINUX
#define UACCESS_PREFIX_linux
#else
#define UACCESS_PREFIX_linux __linux_
#endif

/**
 * Convert virtual address to physical address
 *
 * @v virt		Virtual address
 * @ret phys		Physical address
 */
static inline __always_inline physaddr_t
UACCESS_INLINE ( linux, virt_to_phys ) ( volatile const void *virt ) {

	/* We do not know the real underlying physical address.  We
	 * provide this stub implementation only because it is
	 * required by alloc_memblock() (which allocates memory with
	 * specified physical address alignment).  We assume that the
	 * low-order bits of virtual addresses match the low-order
	 * bits of physical addresses, and so simply returning the
	 * virtual address will suffice for the purpose of determining
	 * alignment.
	 */
	return ( ( physaddr_t ) virt );
}

/**
 * Convert physical address to virtual address
 *
 * @v phys		Physical address
 * @ret virt		Virtual address
 */
static inline __always_inline void *
UACCESS_INLINE ( linux, phys_to_virt ) ( physaddr_t phys ) {

	/* For symmetry with the stub virt_to_phys() */
	return ( ( void * ) phys );
}

static inline __always_inline userptr_t
UACCESS_INLINE ( linux, virt_to_user ) ( volatile const void *addr ) {
	return trivial_virt_to_user ( addr );
}

#endif /* _IPXE_LINUX_UACCESS_H */
