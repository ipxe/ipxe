#ifndef _IPXE_VIRT_OFFSET_H
#define _IPXE_VIRT_OFFSET_H

/**
 * @file
 *
 * Virtual offset memory model
 *
 * No currently supported machine provides a full 64 bits of physical
 * address space.  When we have ownership of the page tables (or
 * segmentation mechanism), we can therefore use the following model:
 *
 *   - For 32-bit builds: set up a circular map so that all 32-bit
 *     virtual addresses are at a fixed offset from the 32-bit
 *     physical addresses.
 *
 *   - For 64-bit builds: identity-map the required portion of the
 *     physical address space, then map iPXE itself using virtual
 *     addresses in the negative (kernel) address space.
 *
 * In both cases, we can define "virt_offset" as "the value to be
 * added to an address within iPXE's own image in order to obtain its
 * physical address".  With this definition:
 *
 *   - For 32-bit builds: conversion between physical and virtual
 *     addresses is a straightforward addition or subtraction of
 *     virt_offset, since the whole 32-bit address space is circular.
 *
 *   - For 64-bit builds: conversion from any valid physical address
 *     is a no-op (since all physical addresses are identity-mapped),
 *     and conversion from a virtual address to a physical address
 *     requires an addition of virt_offset if and only if the virtual
 *     address lies in the negative portion of the address space
 *     (i.e. has the MSB set).
 *
 * For x86_64-pcbios, we identity-map the low 4GB of address space
 * since the only accesses required above 4GB are for MMIO (typically
 * PCI devices with large memory BARs).
 *
 * For riscv64-sbi, we identity-map as much of the physical address
 * space as can be mapped by the paging model (Sv39, Sv48, or Sv57).
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef UACCESS_OFFSET
#define UACCESS_PREFIX_offset
#else
#define UACCESS_PREFIX_offset __offset_
#endif

/** Virtual address offset
 *
 * This is defined to be the value to be added to an address within
 * iPXE's own image in order to obtain its physical address, as
 * described above.
 */
extern const unsigned long virt_offset;

/** Allow for architecture-specific overrides of virt_offset */
#include <bits/virt_offset.h>

/**
 * Convert physical address to virtual address
 *
 * @v phys		Physical address
 * @ret virt		Virtual address
 */
static inline __always_inline void *
UACCESS_INLINE ( offset, phys_to_virt ) ( unsigned long phys ) {

	/* In a 64-bit build, any valid physical address is directly
	 * usable as a virtual address, since physical addresses are
	 * identity-mapped.
	 */
	if ( sizeof ( physaddr_t ) > sizeof ( uint32_t ) )
		return ( ( void * ) phys );

	/* In a 32-bit build: subtract virt_offset */
	return ( ( void * ) ( phys - virt_offset ) );
}

/**
 * Convert virtual address to physical address
 *
 * @v virt		Virtual address
 * @ret phys		Physical address
 */
static inline __always_inline physaddr_t
UACCESS_INLINE ( offset, virt_to_phys ) ( volatile const void *virt ) {
	physaddr_t addr = ( ( physaddr_t ) virt );

	/* In a 64-bit build, any valid virtual address with the MSB
	 * clear is directly usable as a physical address, since it
	 * must lie within the identity-mapped portion.
	 *
	 * This test will typically reduce to a single "branch if less
	 * than zero" instruction.
	 */
	if ( ( sizeof ( physaddr_t ) > sizeof ( uint32_t ) ) &&
	     ( ! ( addr & ( 1ULL << ( 8 * sizeof ( physaddr_t ) - 1 ) ) ) ) ) {
		return addr;
	}

	/* In a 32-bit build or in a 64-bit build with a virtual
	 * address with the MSB set: add virt_offset
	 */
	return ( addr + virt_offset );
}

#endif /* _IPXE_VIRT_OFFSET_H */
