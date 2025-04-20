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
 * implementation of user_to_phys() since this is required by
 * alloc_memblock().  We provide no implementation of phys_to_user();
 * any code attempting to access physical addresses will therefore
 * (correctly) fail to link.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef UACCESS_LINUX
#define UACCESS_PREFIX_linux
#else
#define UACCESS_PREFIX_linux __linux_
#endif

/**
 * Convert user pointer to physical address
 *
 * @v userptr		User pointer
 * @v offset		Offset from user pointer
 * @ret phys_addr	Physical address
 */
static inline __always_inline unsigned long
UACCESS_INLINE ( linux, user_to_phys ) ( userptr_t userptr, off_t offset ) {

	/* We do not know the real underlying physical address.  We
	 * provide this stub implementation only because it is
	 * required by alloc_memblock() (which allocates memory with
	 * specified physical address alignment).  We assume that the
	 * low-order bits of virtual addresses match the low-order
	 * bits of physical addresses, and so simply returning the
	 * virtual address will suffice for the purpose of determining
	 * alignment.
	 */
	return ( ( unsigned long ) ( userptr + offset ) );
}

/**
 * Convert physical address to user pointer
 *
 * @v phys_addr		Physical address
 * @ret userptr		User pointer
 */
static inline __always_inline userptr_t
UACCESS_INLINE ( linux, phys_to_user ) ( physaddr_t phys_addr ) {

	/* For symmetry with the stub user_to_phys() */
	return ( ( userptr_t ) phys_addr );
}

static inline __always_inline userptr_t
UACCESS_INLINE ( linux, virt_to_user ) ( volatile const void *addr ) {
	return trivial_virt_to_user ( addr );
}

static inline __always_inline void *
UACCESS_INLINE ( linux, user_to_virt ) ( userptr_t userptr, off_t offset ) {
	return trivial_user_to_virt ( userptr, offset );
}

static inline __always_inline off_t
UACCESS_INLINE ( linux, memchr_user ) ( userptr_t buffer, off_t offset,
					int c, size_t len ) {
	return trivial_memchr_user ( buffer, offset, c, len );
}

#endif /* _IPXE_LINUX_UACCESS_H */
