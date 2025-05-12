/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <byteswap.h>
#include <ipxe/uaccess.h>
#include <ipxe/fdt.h>
#include <ipxe/fdtmem.h>

/** @file
 *
 * Flattened Device Tree memory map
 *
 */

/** Start address of the iPXE image */
extern char _prefix[];

/** Initialised-data size of the iPXE image (defined by linker) */
extern size_t ABS_SYMBOL ( _filesz );
static size_t filesz = ABS_VALUE_INIT ( _filesz );

/** In-memory size of the iPXE image (defined by linker) */
extern size_t ABS_SYMBOL ( _memsz );
static size_t memsz = ABS_VALUE_INIT ( _memsz );

/** Relocation required alignment (defined by prefix or linker) */
extern physaddr_t ABS_SYMBOL ( _max_align );
static physaddr_t max_align = ABS_VALUE_INIT ( _max_align );

/** Colour for debug messages */
#define colour &memsz

/** A memory region descriptor */
struct fdtmem_region {
	/** Region start address */
	physaddr_t start;
	/** Region end address */
	physaddr_t end;
	/** Region flags */
	int flags;
	/** Region name (for debug messages) */
	const char *name;
};

/** Region is usable as RAM */
#define FDTMEM_RAM 0x0001

/**
 * Update memory region descriptor
 *
 * @v region		Memory region of interest to be updated
 * @v start		Start address of this region
 * @v size		Size of this region
 * @v flags		Region flags
 * @v name		Region name (for debugging)
 */
static void fdtmem_update ( struct fdtmem_region *region, uint64_t start,
			    uint64_t size, int flags, const char *name ) {
	uint64_t end;

	/* Ignore empty regions */
	if ( ! size )
		return;

	/* Calculate end address (and truncate if necessary) */
	end = ( start + size - 1 );
	if ( end < start ) {
		end = ~( ( uint64_t ) 0 );
		DBGC ( colour, "FDTMEM [%#08llx,%#08llx] %s truncated "
		       "(invalid size %#08llx)\n",
		       ( ( unsigned long long ) start ),
		       ( ( unsigned long long ) end ), name,
		       ( ( unsigned long long ) size ) );
	}

	/* Ignore regions entirely below the region of interest */
	if ( end < region->start )
		return;

	/* Ignore regions entirely above the region of interest */
	if ( start > region->end )
		return;

	/* Update region of interest as applicable */
	if ( start <= region->start ) {

		/* This region covers the region of interest */
		region->flags = flags;
		if ( DBG_LOG )
			region->name = name;

		/* Update end address if no closer boundary exists */
		if ( end < region->end )
			region->end = end;

	} else if ( start < region->end ) {

		/* Update end address if no closer boundary exists */
		region->end = ( start - 1 );
	}
}

/**
 * Update memory region descriptor based on device tree node
 *
 * @v region		Memory region of interest to be updated
 * @v fdt		Device tree
 * @v offset		Starting node offset
 * @v match		Required device type (or NULL)
 * @v flags		Region flags
 * @ret rc		Return status code
 */
static int fdtmem_update_node ( struct fdtmem_region *region, struct fdt *fdt,
				unsigned int offset, const char *match,
				int flags ) {
	struct fdt_descriptor desc;
	struct fdt_reg_cells regs;
	const char *devtype;
	uint64_t start;
	uint64_t size;
	int depth;
	int count;
	int index;
	int rc;

	/* Parse region cell sizes */
	fdt_reg_cells ( fdt, offset, &regs );

	/* Scan through reservations */
	for ( depth = -1 ; ; depth += desc.depth, offset = desc.next ) {

		/* Describe token */
		if ( ( rc = fdt_describe ( fdt, offset, &desc ) ) != 0 ) {
			DBGC ( colour, "FDTMEM has malformed node: %s\n",
			       strerror ( rc ) );
			return rc;
		}

		/* Terminate when we exit this node */
		if ( ( depth == 0 ) && ( desc.depth < 0 ) )
			break;

		/* Ignore any non-immediate child nodes */
		if ( ! ( ( depth == 0 ) && desc.name && ( ! desc.data ) ) )
			continue;

		/* Ignore any non-matching children */
		if ( match ) {
			devtype = fdt_string ( fdt, desc.offset,
					       "device_type" );
			if ( ! devtype )
				continue;
			if ( strcmp ( devtype, match ) != 0 )
				continue;
		}

		/* Count regions */
		count = fdt_reg_count ( fdt, desc.offset, &regs );
		if ( count < 0 ) {
			rc = count;
			DBGC ( colour, "FDTMEM has malformed region %s: %s\n",
			       desc.name, strerror ( rc ) );
			continue;
		}

		/* Scan through this region */
		for ( index = 0 ; index < count ; index++ ) {

			/* Get region starting address and size */
			if ( ( rc = fdt_reg_address ( fdt, desc.offset, &regs,
						      index, &start ) ) != 0 ){
				DBGC ( colour, "FDTMEM %s region %d has "
				       "malformed start address: %s\n",
				       desc.name, index, strerror ( rc ) );
				break;
			}
			if ( ( rc = fdt_reg_size ( fdt, desc.offset, &regs,
						   index, &size ) ) != 0 ) {
				DBGC ( colour, "FDTMEM %s region %d has "
				       "malformed size: %s\n",
				       desc.name, index, strerror ( rc ) );
				break;
			}

			/* Update memory region descriptor */
			fdtmem_update ( region, start, size, flags,
					desc.name );
		}
	}

	return 0;
}

/**
 * Update memory region descriptor based on device tree
 *
 * @v region		Memory region of interest to be updated
 * @v fdt		Device tree
 * @ret rc		Return status code
 */
static int fdtmem_update_tree ( struct fdtmem_region *region,
				struct fdt *fdt ) {
	const struct fdt_reservation *rsv;
	unsigned int offset;
	int rc;

	/* Update based on memory regions in the root node */
	if ( ( rc = fdtmem_update_node ( region, fdt, 0, "memory",
					 FDTMEM_RAM ) ) != 0 )
		return rc;

	/* Update based on memory reservations block */
	for_each_fdt_reservation ( rsv, fdt ) {
		fdtmem_update ( region, be64_to_cpu ( rsv->start ),
				be64_to_cpu ( rsv->size ), 0, "<rsv>" );
	}

	/* Locate reserved-memory node */
	if ( ( rc = fdt_path ( fdt, "/reserved-memory", &offset ) ) != 0 ) {
		DBGC ( colour, "FDTMEM could not locate /reserved-memory: "
		       "%s\n", strerror ( rc ) );
		return rc;
	}

	/* Update based on memory regions in the reserved-memory node */
	if ( ( rc = fdtmem_update_node ( region, fdt, offset, NULL,
					 0 ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Find a relocation address for iPXE
 *
 * @v hdr		FDT header
 * @v limit		Size of accessible physical address space (or zero)
 * @ret new		New physical address for relocation
 *
 * Find a suitably aligned address towards the top of existent memory
 * to which iPXE may be relocated, along with a copy of the system
 * device tree.
 *
 * This function may be called very early in initialisation, before
 * .data is writable or .bss has been zeroed.  Neither this function
 * nor any function that it calls may write to or rely upon the zero
 * initialisation of any static variables.
 */
physaddr_t fdtmem_relocate ( struct fdt_header *hdr, size_t limit ) {
	struct fdt fdt;
	struct fdtmem_region region;
	physaddr_t old;
	physaddr_t new;
	physaddr_t try;
	size_t len;
	void *dest;
	int rc;

	/* Sanity check */
	assert ( ( max_align & ( max_align - 1 ) ) == 0 );

	/* Get current physical address */
	old = virt_to_phys ( _prefix );

	/* Parse FDT */
	if ( ( rc = fdt_parse ( &fdt, hdr, -1UL ) ) != 0 ) {
		DBGC ( colour, "FDTMEM could not parse FDT: %s\n",
		       strerror ( rc ) );
		/* Refuse relocation if we have no FDT */
		return old;
	}

	/* Determine required length */
	assert ( memsz > 0 );
	assert ( ( memsz % FDT_MAX_ALIGN ) == 0 );
	len = ( memsz + fdt.len );
	assert ( len > 0 );
	DBGC ( colour, "FDTMEM requires %#zx + %#zx => %#zx bytes for "
	       "relocation\n", memsz, fdt.len, len );

	/* Construct memory map and choose a relocation address */
	region.start = 0;
	new = old;
	while ( 1 ) {

		/* Initialise region */
		region.end = ~( ( physaddr_t ) 0 );
		region.flags = 0;
		region.name = "<empty>";

		/* Update region based on device tree */
		if ( ( rc = fdtmem_update_tree ( &region, &fdt ) ) != 0 )
			break;

		/* Treat existing iPXE image as reserved */
		fdtmem_update ( &region, old, memsz, 0, "iPXE" );

		/* Treat existing device tree as reserved */
		fdtmem_update ( &region, virt_to_phys ( hdr ), fdt.len, 0,
				"FDT" );

		/* Treat inaccessible physical memory as reserved */
		if ( limit ) {
			fdtmem_update ( &region, limit, -limit, 0,
					"<inaccessible>" );
		}

		/* Dump region descriptor (for debugging) */
		DBGC ( colour, "FDTMEM [%#08lx,%#08lx] %s\n",
		       region.start, region.end, region.name );
		assert ( region.end >= region.start );

		/* Use highest possible region */
		if ( ( region.flags & FDTMEM_RAM ) &&
		     ( ( region.end - region.start ) > len ) ) {

			/* Determine candidate address after alignment */
			try = ( ( region.end - len - 1 ) &
				~( max_align - 1 ) );

			/* Use this address if within region */
			if ( try >= region.start )
				new = try;
		}

		/* Move to next region */
		region.start = ( region.end + 1 );
		if ( ! region.start )
			break;
	}

	/* Copy iPXE and device tree to new location */
	if ( new != old ) {
		dest = phys_to_virt ( new );
		memset ( dest, 0, len );
		memcpy ( dest, _prefix, filesz );
		memcpy ( ( dest + memsz ), hdr, fdt.len );
	}

	DBGC ( colour, "FDTMEM relocating %#08lx => [%#08lx,%#08lx]\n",
	       old, new, ( ( physaddr_t ) ( new + len - 1 ) ) );
	return new;
}
