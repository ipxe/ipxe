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
#include <ipxe/memmap.h>
#include <ipxe/io.h>
#include <ipxe/fdt.h>
#include <ipxe/fdtmem.h>

/** @file
 *
 * Flattened Device Tree memory map
 *
 */

/** Start address of the iPXE image */
extern char _prefix[];

/** End address of the iPXE image */
extern char _end[];

/** Total in-memory size (calculated by linker) */
extern size_t ABS_SYMBOL ( _memsz );
static size_t memsz = ABS_VALUE_INIT ( _memsz );

/** Relocation required alignment (defined by prefix or linker) */
extern size_t ABS_SYMBOL ( _max_align );
static size_t max_align = ABS_VALUE_INIT ( _max_align );

/** In-use memory region for iPXE and system device tree copy */
struct used_region fdtmem_used __used_region = {
	.name = "iPXE/FDT",
};

/** Maximum accessible physical address */
static physaddr_t fdtmem_max;

/** Maximum 32-bit physical address */
#define FDTMEM_MAX32 0xffffffff

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
static int fdtmem_update_node ( struct memmap_region *region, struct fdt *fdt,
				unsigned int offset, const char *match,
				unsigned int flags ) {
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
			DBGC ( region, "FDTMEM has malformed node: %s\n",
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
			if ( flags & MEMMAP_FL_RESERVED ) {
				/* Assume this is a non-fixed reservation */
				continue;
			}
			rc = count;
			DBGC ( region, "FDTMEM has malformed region %s: %s\n",
			       desc.name, strerror ( rc ) );
			continue;
		}

		/* Scan through this region */
		for ( index = 0 ; index < count ; index++ ) {

			/* Get region starting address and size */
			if ( ( rc = fdt_reg_address ( fdt, desc.offset, &regs,
						      index, &start ) ) != 0 ){
				DBGC ( region, "FDTMEM %s region %d has "
				       "malformed start address: %s\n",
				       desc.name, index, strerror ( rc ) );
				break;
			}
			if ( ( rc = fdt_reg_size ( fdt, desc.offset, &regs,
						   index, &size ) ) != 0 ) {
				DBGC ( region, "FDTMEM %s region %d has "
				       "malformed size: %s\n",
				       desc.name, index, strerror ( rc ) );
				break;
			}

			/* Update memory region descriptor */
			memmap_update ( region, start, size, flags,
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
static int fdtmem_update_tree ( struct memmap_region *region,
				struct fdt *fdt ) {
	const struct fdt_reservation *rsv;
	unsigned int offset;
	int rc;

	/* Update based on memory regions in the root node */
	if ( ( rc = fdtmem_update_node ( region, fdt, 0, "memory",
					 MEMMAP_FL_MEMORY ) ) != 0 )
		return rc;

	/* Update based on memory reservations block */
	for_each_fdt_reservation ( rsv, fdt ) {
		memmap_update ( region, be64_to_cpu ( rsv->start ),
				be64_to_cpu ( rsv->size ), MEMMAP_FL_RESERVED,
				NULL );
	}

	/* Locate reserved-memory node */
	if ( ( rc = fdt_path ( fdt, "/reserved-memory", &offset ) ) != 0 ) {
		DBGC ( region, "FDTMEM could not locate /reserved-memory: "
		       "%s\n", strerror ( rc ) );
		return rc;
	}

	/* Update based on memory regions in the reserved-memory node */
	if ( ( rc = fdtmem_update_node ( region, fdt, offset, NULL,
					 MEMMAP_FL_RESERVED ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Describe memory region
 *
 * @v min		Minimum address
 * @v max		Maximum accessible physical address
 * @v fdt		Device tree
 * @v region		Region descriptor to fill in
 */
static void fdtmem_describe ( uint64_t min, uint64_t max, struct fdt *fdt,
			      struct memmap_region *region ) {
	uint64_t inaccessible;

	/* Initialise region */
	memmap_init ( min, region );

	/* Update region based on device tree */
	fdtmem_update_tree ( region, fdt );

	/* Treat inaccessible physical memory as such */
	inaccessible = ( max + 1 );
	memmap_update ( region, inaccessible, -inaccessible,
			MEMMAP_FL_INACCESSIBLE, NULL );
}

/**
 * Get length for copy of iPXE and device tree
 *
 * @v fdt		Device tree
 * @ret len		Total length
 */
static size_t fdtmem_len ( struct fdt *fdt ) {
	size_t len;

	/* Calculate total length and check device tree alignment */
	len = ( memsz + fdt->len );
	assert ( ( memsz % FDT_MAX_ALIGN ) == 0 );

	/* Align length.  Not technically necessary, but keeps the
	 * resulting memory maps looking relatively sane.
	 */
	len = ( ( len + PAGE_SIZE - 1 ) & ~( PAGE_SIZE - 1 ) );

	return len;
}

/**
 * Find a relocation address for iPXE
 *
 * @v hdr		FDT header
 * @v max		Maximum accessible physical address
 * @ret new		New physical address for relocation
 *
 * Find a suitably aligned address towards the top of existent 32-bit
 * memory to which iPXE may be relocated, along with a copy of the
 * system device tree.
 *
 * This function may be called very early in initialisation, before
 * .data is writable or .bss has been zeroed.  Neither this function
 * nor any function that it calls may write to or rely upon the zero
 * initialisation of any static variables.
 */
physaddr_t fdtmem_relocate ( struct fdt_header *hdr, physaddr_t max ) {
	struct fdt fdt;
	struct memmap_region region;
	physaddr_t addr;
	physaddr_t next;
	physaddr_t old;
	physaddr_t new;
	physaddr_t try;
	size_t len;
	int rc;

	/* Sanity check */
	assert ( ( max_align & ( max_align - 1 ) ) == 0 );

	/* Get current physical address */
	old = virt_to_phys ( _prefix );

	/* Parse FDT */
	if ( ( rc = fdt_parse ( &fdt, hdr, -1UL ) ) != 0 ) {
		DBGC ( hdr, "FDTMEM could not parse FDT: %s\n",
		       strerror ( rc ) );
		/* Refuse relocation if we have no FDT */
		return old;
	}

	/* Determine required length */
	len = fdtmem_len ( &fdt );
	assert ( len > 0 );
	DBGC ( hdr, "FDTMEM requires %#zx + %#zx => %#zx bytes for "
	       "relocation\n", memsz, fdt.len, len );

	/* Limit relocation to 32-bit address space
	 *
	 * Devices with only 32-bit DMA addressing are relatively
	 * common even on systems with 64-bit CPUs.  Limit relocation
	 * of iPXE to 32-bit address space so that I/O buffers and
	 * other DMA allocations will be accessible by 32-bit devices.
	 */
	if ( max > FDTMEM_MAX32 )
		max = FDTMEM_MAX32;

	/* Construct memory map and choose a relocation address */
	new = old;
	for ( addr = 0, next = 1 ; next ; addr = next ) {

		/* Describe region and in-use memory */
		fdtmem_describe ( addr, max, &fdt, &region );
		memmap_update ( &region, old, memsz, MEMMAP_FL_USED, "iPXE" );
		memmap_update ( &region, virt_to_phys ( hdr ), fdt.len,
				MEMMAP_FL_RESERVED, "FDT" );
		next = ( region.max + 1 );

		/* Dump region descriptor (for debugging) */
		DBGC_MEMMAP ( hdr, &region );
		assert ( region.max >= region.min );

		/* Use highest possible region */
		if ( memmap_is_usable ( &region ) &&
		     ( ( next == 0 ) || ( next >= len ) ) ) {

			/* Determine candidate address after alignment */
			try = ( ( next - len ) & ~( max_align - 1 ) );

			/* Use this address if within region */
			if ( try >= addr )
				new = try;
		}
	}

	DBGC ( hdr, "FDTMEM relocating %#08lx => [%#08lx,%#08lx]\n",
	       old, new, ( ( physaddr_t ) ( new + len - 1 ) ) );
	return new;
}

/**
 * Copy and register system device tree
 *
 * @v hdr		FDT header
 * @v max		Maximum accessible physical address
 * @ret rc		Return status code
 */
int fdtmem_register ( struct fdt_header *hdr, physaddr_t max ) {
	struct fdt_header *copy;
	struct fdt fdt;
	int rc;

	/* Record maximum accessible physical address */
	fdtmem_max = max;

	/* Parse FDT to obtain length */
	if ( ( rc = fdt_parse ( &fdt, hdr, -1UL ) ) != 0 ) {
		DBGC ( hdr, "FDTMEM could not parse FDT: %s\n",
		       strerror ( rc ) );
		return rc;
	}

	/* Copy device tree to end of iPXE image */
	copy = ( ( void * ) _end );
	memcpy ( copy, hdr, fdt.len );

	/* Update in-use memory region */
	memmap_use ( &fdtmem_used, virt_to_phys ( _prefix ),
		     fdtmem_len ( &fdt ) );

	/* Register copy as system device tree */
	if ( ( rc = fdt_parse ( &sysfdt, copy, -1UL ) ) != 0 ) {
		DBGC ( hdr, "FDTMEM could not register FDT: %s\n",
		       strerror ( rc ) );
		return rc;
	}
	assert ( sysfdt.len == fdt.len );

	/* Dump system memory map (for debugging) */
	memmap_dump_all ( 1 );

	return 0;
}

/**
 * Describe memory region from system memory map
 *
 * @v min		Minimum address
 * @v hide		Hide in-use regions from the memory map
 * @v region		Region descriptor to fill in
 */
static void fdtmem_describe_region ( uint64_t min, int hide,
				     struct memmap_region *region ) {

	/* Describe memory region based on device tree */
	fdtmem_describe ( min, fdtmem_max, &sysfdt, region );

	/* Update memory region based on in-use regions, if applicable */
	if ( hide )
		memmap_update_used ( region );
}

PROVIDE_MEMMAP ( fdt, memmap_describe, fdtmem_describe_region );
PROVIDE_MEMMAP_INLINE ( fdt, memmap_sync );
