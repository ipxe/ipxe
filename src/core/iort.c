/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
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
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <errno.h>
#include <ipxe/pci.h>
#include <ipxe/iort.h>

/** @file
 *
 * I/O remapping table (IORT)
 *
 */

/**
 * Locate I/O remapping table
 *
 * @ret iort		I/O remapping table, or NULL if not found
 */
static const struct iort_table * iort_find ( void ) {
	const struct iort_table *iort;

	/* Locate table */
	iort = container_of ( acpi_table ( IORT_SIGNATURE, 0 ),
			      struct iort_table, acpi );
	if ( ! iort ) {
		DBGC ( IORT_SIGNATURE, "IORT not found\n" );
		return NULL;
	}

	return iort;
}

/**
 * Get I/O remapping table node
 *
 * @v iort		I/O remapping table
 * @v offset		Node offset
 * @ret node		Node, or NULL on error
 */
static const struct iort_node * iort_node ( const struct iort_table *iort,
					    size_t offset ) {
	const struct iort_node *node;
	const struct iort_ids *ids;
	size_t remaining;
	size_t len;
	unsigned int count;
	unsigned int max;

	/* Check that node lies within table */
	len = le32_to_cpu ( iort->acpi.length );
	if ( offset > len ) {
		DBGC ( iort, "IORT node +%#zx is outside table\n", offset );
		return NULL;
	}
	remaining = ( len - offset );
	if ( sizeof ( *node ) > remaining ) {
		DBGC ( iort, "IORT node +%#zx exceeds table\n", offset );
		return NULL;
	}
	node = ( ( ( const void * ) iort ) + offset );
	len = le16_to_cpu ( node->len );
	if ( len > remaining ) {
		DBGC ( iort, "IORT node +%#zx exceeds table\n", offset );
		return NULL;
	}
	if ( len < sizeof ( *node ) ) {
		DBGC ( iort, "IORT node +%#zx too short\n", offset );
		return NULL;
	}

	/* Check that ID mapping lies within node */
	if ( le32_to_cpu ( node->offset ) > len ) {
		DBGC ( iort, "IORT node +%#zx ID mapping is outside node\n",
		       offset );
		return NULL;
	}
	remaining = ( len - le32_to_cpu ( node->offset ) );
	max = ( remaining / sizeof ( ids[0] ) );
	count = le32_to_cpu ( node->count );
	if ( count > max ) {
		DBGC ( iort, "IORT node +%#zx ID mapping exceeds node\n",
		       offset );
		return NULL;
	}

	DBGC2 ( iort, "IORT node +%#zx type %d\n", offset, node->type );
	return node;
}

/**
 * Map PCI device
 *
 * @v pci		PCI device
 * @v map		I/O remapping map to fill in
 * @ret rc		Return status code
 */
int iort_map_pci ( struct pci_device *pci, struct iort_mapping *map ) {
	const struct iort_table *iort;
	const struct iort_node *node;
	const struct iort_node_pci *pnode;
	const struct iort_node *out;
	const struct iort_ids *ids;
	unsigned int node_count;
	unsigned int id_count;
	size_t offset;
	size_t len;
	uint32_t id;

	/* Find IORT table */
	iort = iort_find();
	if ( ! iort )
		return -ENOENT;

	/* Find a matching PCI node */
	for ( node_count = le32_to_cpu ( iort->count ),
	      offset = le32_to_cpu ( iort->offset ) ;
	      node_count && ( ( node = iort_node ( iort, offset ) ) != NULL ) ;
	      node_count--, offset += len ) {

		/* Skip non-PCI nodes */
		len = le16_to_cpu ( node->len );
		if ( node->type != IORT_NODE_PCI )
			continue;
		if ( len < sizeof ( *pnode ) ) {
			DBGC ( iort, "IORT node +%#zx too short for PCI\n",
			       offset );
			continue;
		}
		pnode = container_of ( node, struct iort_node_pci, node );

		/* Find a matching bus:dev.fn address range */
		ids = ( ( ( const void * ) node ) +
			le32_to_cpu ( node->offset ) );
		for ( id_count = le32_to_cpu ( node->count ) ; id_count ;
		      id_count--, ids++ ) {

			/* Skip non-matching bus:dev.fn addresses */
			id = ( pci->busdevfn - le32_to_cpu ( ids->input ) -
			       PCI_BUSDEVFN ( le32_to_cpu ( pnode->segment ),
					      0, 0, 0 ) );
			if ( id > le32_to_cpu ( ids->limit ) )
				continue;

			/* Identify output node */
			out = iort_node ( iort, le32_to_cpu ( ids->offset ) );
			if ( ! out )
				return -EINVAL;

			/* Identify output ID */
			if ( ids->flags & cpu_to_le32 ( IORT_FL_SINGLE ) )
				id = 0;
			id += le32_to_cpu ( ids->output );

			/* Populate mapping */
			map->input = node;
			map->ids = ids;
			map->output = out;
			map->id = id;
			DBGC ( iort, "IORT node +%#zx maps " PCI_FMT " => "
			       "+%#x type %d ID %#x\n", offset,
			       PCI_ARGS ( pci ), le32_to_cpu ( ids->offset ),
			       out->type, map->id );
			DBGC2_HDA ( iort, le32_to_cpu ( ids->offset ),
				    out, le16_to_cpu ( out->len ) );
			return 0;
		}
	}

	return -ENOENT;
}
