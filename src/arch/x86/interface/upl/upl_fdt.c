/*
 * Copyright (C) 2026 Bejean Mosher <bejean.mosher@intel.com>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
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

/** @file
 *
 * Universal Payload device tree
 *
 * The bootloader passes a Flattened Device Tree (as done by EDK2 when
 * built with PcdHandOffFdtEnable).  The device tree uses the node
 * names and properties consumed by EDK2's FdtParserLib.
 *
 */

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <ipxe/memmap.h>
#include <ipxe/fdt.h>
#include <ipxe/fdtmem.h>
#include <ipxe/upl.h>

/**
 * A device tree node visitor
 *
 * @v offset		Node offset
 * @v name		Node name
 * @v ctx		Visitor context
 * @ret rc		Return status code (or positive to stop the walk)
 */
typedef int ( * upl_fdt_visit_t ) ( unsigned int offset, const char *name,
				    void *ctx );

/**
 * Visit nodes
 *
 * @v offset		Starting node offset
 * @v recurse		Visit all descendants, rather than only children
 * @v visit		Node visitor
 * @v ctx		Visitor context
 * @ret rc		Return status code (or positive if stopped)
 */
static int upl_fdt_walk ( unsigned int offset, int recurse,
			  upl_fdt_visit_t visit, void *ctx ) {
	struct fdt_descriptor desc;
	int depth;
	int rc;

	/* Do nothing if we have no device tree */
	if ( ! sysfdt.hdr )
		return -ENODEV;

	for ( depth = -1 ; ; depth += desc.depth, offset = desc.next ) {

		/* Describe token */
		if ( ( rc = fdt_describe ( &sysfdt, offset, &desc ) ) != 0 ) {
			DBGC ( &sysfdt, "UPL FDT malformed: %s\n",
			       strerror ( rc ) );
			return rc;
		}

		/* Terminate when we exit the starting node */
		if ( ( depth == 0 ) && ( desc.depth < 0 ) )
			return 0;

		/* Visit applicable nodes */
		if ( desc.name && ( ! desc.data ) && ( depth >= 0 ) &&
		     ( recurse || ( depth == 0 ) ) ) {
			if ( ( rc = visit ( desc.offset, desc.name,
					    ctx ) ) != 0 )
				return rc;
		}
	}
}

/**
 * Get region
 *
 * @v offset		Node offset
 * @v index		Region index
 * @v start		Starting address to fill in
 * @v size		Size to fill in
 * @ret rc		Return status code
 */
static int upl_fdt_reg ( unsigned int offset, unsigned int index,
			 uint64_t *start, uint64_t *size ) {
	struct fdt_reg_cells regs;
	int rc;

	if ( ( rc = fdt_parent_reg_cells ( &sysfdt, offset, &regs ) ) != 0 )
		return rc;
	if ( ( rc = fdt_reg_address ( &sysfdt, offset, &regs, index,
				      start ) ) != 0 )
		return rc;
	if ( ( rc = fdt_reg_size ( &sysfdt, offset, &regs, index,
				   size ) ) != 0 )
		return rc;

	return 0;
}

/** A node search */
struct upl_fdt_search {
	/** Node name prefix, or NULL */
	const char *prefix;
	/** Compatible string, or NULL */
	const char *compatible;
	/** Number of matching nodes to skip */
	unsigned int skip;
	/** Matching node offset */
	unsigned int offset;
};

/**
 * Check for matching node
 *
 * @v offset		Node offset
 * @v name		Node name
 * @v ctx		Node search
 * @ret rc		Return status code (or positive if matched)
 */
static int upl_fdt_match ( unsigned int offset, const char *name,
			   void *ctx ) {
	struct upl_fdt_search *search = ctx;
	const char *compatible;

	if ( search->prefix &&
	     ( strncmp ( name, search->prefix,
			 strlen ( search->prefix ) ) != 0 ) ) {
		return 0;
	}
	if ( search->compatible ) {
		compatible = fdt_string ( &sysfdt, offset, "compatible" );
		if ( ! ( compatible &&
			 ( strcmp ( compatible, search->compatible ) == 0 ) ) )
			return 0;
	}
	if ( search->skip ) {
		search->skip--;
		return 0;
	}
	search->offset = offset;
	return 1;
}

/**
 * Find first node by name prefix and/or compatible string
 *
 * @v prefix		Node name prefix, or NULL
 * @v compatible	Compatible string, or NULL
 * @v offset		Node offset to fill in
 * @ret rc		Return status code
 */
int upl_fdt_find ( const char *prefix, const char *compatible,
		   unsigned int *offset ) {
	struct upl_fdt_search search = {
		.prefix = prefix,
		.compatible = compatible,
	};

	if ( upl_fdt_walk ( 0, 1, upl_fdt_match, &search ) <= 0 )
		return -ENOENT;
	*offset = search.offset;
	return 0;
}

/**
 * Find start of reserved memory region by compatible string
 *
 * @v compatible	Compatible string (e.g. "acpi")
 * @v index		Index of region with this compatible string
 * @v start		Starting address to fill in
 * @ret rc		Return status code
 */
int upl_fdt_reserved ( const char *compatible, unsigned int index,
		       uint64_t *start ) {
	struct upl_fdt_search search = {
		.compatible = compatible,
		.skip = index,
	};
	unsigned int offset;
	uint64_t size;
	int rc;

	if ( ! sysfdt.hdr )
		return -ENODEV;
	if ( ( rc = fdt_path ( &sysfdt, "/reserved-memory", &offset ) ) != 0 )
		return rc;
	if ( upl_fdt_walk ( offset, 0, upl_fdt_match, &search ) <= 0 )
		return -ENOENT;
	return upl_fdt_reg ( search.offset, 0, start, &size );
}

/**
 * Install iPXE as a universal payload
 *
 * @v hdr		Device tree provided by the bootloader
 */
__asmcall void upl_install ( struct fdt_header *hdr ) {

	/* Copy and register system device tree */
	fdtmem_register ( hdr, ( upl_mapped - 1 ) );
}
