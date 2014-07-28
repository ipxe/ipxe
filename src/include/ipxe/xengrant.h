#ifndef _IPXE_XENGRANT_H
#define _IPXE_XENGRANT_H

/** @file
 *
 * Xen grant tables
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <ipxe/io.h>
#include <ipxe/xen.h>
#include <xen/grant_table.h>

/**
 * Query grant table size
 *
 * @v xen		Xen hypervisor
 * @v size		Table size
 * @ret xenrc		Xen status code
 */
static inline __attribute__ (( always_inline )) int
xengrant_query_size ( struct xen_hypervisor *xen,
		      struct gnttab_query_size *size ) {

	return xen_hypercall_3 ( xen, __HYPERVISOR_grant_table_op,
				 GNTTABOP_query_size,
				 virt_to_phys ( size ), 1 );
}

/**
 * Set grant table version
 *
 * @v xen		Xen hypervisor
 * @v version		Version
 * @ret xenrc		Xen status code
 */
static inline __attribute__ (( always_inline )) int
xengrant_set_version ( struct xen_hypervisor *xen,
		       struct gnttab_set_version *version ) {

	return xen_hypercall_3 ( xen, __HYPERVISOR_grant_table_op,
				 GNTTABOP_set_version,
				 virt_to_phys ( version ), 1 );
}

/**
 * Invalidate access to a page
 *
 * @v xen		Xen hypervisor
 * @v ref		Grant reference
 */
static inline __attribute__ (( always_inline )) void
xengrant_invalidate ( struct xen_hypervisor *xen, grant_ref_t ref ) {
	union grant_entry_v2 *entry = &xen->grant.table[ref];

	/* Sanity check */
	assert ( ( readw ( &entry->hdr.flags ) &
		   ( GTF_reading | GTF_writing ) ) == 0 );

	/* This should apparently be done using a cmpxchg instruction.
	 * We omit this: partly in the interests of simplicity, but
	 * mainly since our control flow generally does not permit
	 * failure paths to themselves fail.
	 */
	writew ( 0, &entry->hdr.flags );
}

/**
 * Permit access to a page
 *
 * @v xen		Xen hypervisor
 * @v ref		Grant reference
 * @v domid		Domain ID
 * @v subflags		Additional flags
 * @v page		Page start
 */
static inline __attribute__ (( always_inline )) void
xengrant_permit_access ( struct xen_hypervisor *xen, grant_ref_t ref,
			 domid_t domid, unsigned int subflags, void *page ) {
	union grant_entry_v2 *entry = &xen->grant.table[ref];
	unsigned long frame = ( virt_to_phys ( page ) / PAGE_SIZE );

	writew ( domid, &entry->full_page.hdr.domid );
	if ( sizeof ( physaddr_t ) == sizeof ( uint64_t ) ) {
		writeq ( frame, &entry->full_page.frame );
	} else {
		writel ( frame, &entry->full_page.frame );
	}
	wmb();
	writew ( ( GTF_permit_access | subflags ), &entry->full_page.hdr.flags);
	wmb();
}

extern int xengrant_alloc ( struct xen_hypervisor *xen, grant_ref_t *refs,
			    unsigned int count );
extern void xengrant_free ( struct xen_hypervisor *xen, grant_ref_t *refs,
			    unsigned int count );

#endif /* _IPXE_XENGRANT_H */
