/*
 * Copyright (C) 2014 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <ipxe/io.h>
#include <ipxe/xen.h>
#include <ipxe/xengrant.h>

/** @file
 *
 * Xen grant tables
 *
 */

/**
 * Allocate grant references
 *
 * @v xen		Xen hypervisor
 * @v refs		Grant references to fill in
 * @v count		Number of references
 * @ret rc		Return status code
 */
int xengrant_alloc ( struct xen_hypervisor *xen, grant_ref_t *refs,
		     unsigned int count ) {
	union grant_entry_v2 *entry;
	unsigned int mask = ( xen->grant.count - 1 );
	unsigned int check = 0;
	unsigned int avail;
	unsigned int ref;

	/* Fail unless we have enough references available */
	avail = ( xen->grant.count - xen->grant.used -
		  GNTTAB_NR_RESERVED_ENTRIES );
	if ( avail < count ) {
		DBGC ( xen, "XENGRANT cannot allocate %d references (only %d "
		       "of %d available)\n", count, avail, xen->grant.count );
		return -ENOBUFS;
	}
	DBGC ( xen, "XENGRANT allocating %d references (from %d of %d "
	       "available)\n", count, avail, xen->grant.count );

	/* Update number of references used */
	xen->grant.used += count;

	/* Find unused references */
	for ( ref = xen->grant.ref ; count ; ref = ( ( ref + 1 ) & mask ) ) {

		/* Sanity check */
		assert ( check++ < xen->grant.count );

		/* Skip reserved references */
		if ( ref < GNTTAB_NR_RESERVED_ENTRIES )
			continue;

		/* Skip in-use references */
		entry = &xen->grant.table[ref];
		if ( readw ( &entry->hdr.flags ) & GTF_type_mask )
			continue;
		if ( readw ( &entry->hdr.domid ) == DOMID_SELF )
			continue;

		/* Mark reference as in-use.  We leave the flags as
		 * empty (to avoid creating a valid grant table entry)
		 * and set the domid to DOMID_SELF.
		 */
		writew ( DOMID_SELF, &entry->hdr.domid );
		DBGC2 ( xen, "XENGRANT allocated ref %d\n", ref );

		/* Record reference */
		refs[--count] = ref;
	}

	/* Update cursor */
	xen->grant.ref = ref;

	return 0;
}

/**
 * Free grant references
 *
 * @v xen		Xen hypervisor
 * @v refs		Grant references
 * @v count		Number of references
 */
void xengrant_free ( struct xen_hypervisor *xen, grant_ref_t *refs,
		     unsigned int count ) {
	union grant_entry_v2 *entry;
	unsigned int ref;
	unsigned int i;

	/* Free references */
	for ( i = 0 ; i < count ; i++ ) {

		/* Sanity check */
		ref = refs[i];
		assert ( ref < xen->grant.count );

		/* Mark reference as unused */
		entry = &xen->grant.table[ref];
		writew ( 0, &entry->hdr.flags );
		writew ( 0, &entry->hdr.domid );
		DBGC2 ( xen, "XENGRANT freed ref %d\n", ref );
	}
}
