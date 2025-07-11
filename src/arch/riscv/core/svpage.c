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
#include <strings.h>
#include <assert.h>
#include <ipxe/hart.h>
#include <ipxe/iomap.h>

/** @file
 *
 * Supervisor page table management
 *
 * With the 64-bit paging schemes (Sv39, Sv48, and Sv57) we choose to
 * identity-map as much as possible of the physical address space via
 * PTEs 0-255, and place a recursive page table entry in PTE 511 which
 * allows PTEs 256-510 to be used to map 1GB "gigapages" within the
 * top 256GB of the 64-bit address space.  At least one of these PTEs
 * will already be in use to map iPXE itself.  The remaining PTEs may
 * be used to map I/O devices.
 */

/** A page table */
struct page_table {
	/** Page table entry */
	uint64_t pte[512];
};

/** Page table entry flags */
enum pte_flags {
	/** Page table entry is valid */
	PTE_V = 0x01,
	/** Page is readable */
	PTE_R = 0x02,
	/** Page is writable */
	PTE_W = 0x04,
	/** Page has been accessed */
	PTE_A = 0x40,
	/** Page is dirty */
	PTE_D = 0x80,
	/** Page is the last page in an allocation
	 *
	 * This bit is ignored by the hardware.  We use it to track
	 * the size of allocations made by ioremap().
	 */
	PTE_LAST = 0x100,
};

/** Page-based memory type (Svpbmt) */
#define PTE_SVPBMT( x ) ( ( ( unsigned long long ) (x) ) << 61 )

/** Page is non-cacheable memory (Svpbmt) */
#define PTE_SVPBMT_NC PTE_SVPBMT ( 1 )

/** Page maps I/O addresses (Svpbmt) */
#define PTE_SVPBMT_IO PTE_SVPBMT ( 2 )

/** Page table entry address */
#define PTE_PPN( addr ) ( (addr) >> 2 )

/** The page table */
extern struct page_table page_table;

/** I/O page size
 *
 * We choose to use 1GB "gigapages", since these are supported by all
 * paging levels.
 */
#define MAP_PAGE_SIZE 0x40000000UL

/** I/O page base address
 *
 * The recursive page table entry maps the high 512GB of the 64-bit
 * address space as 1GB "gigapages".
 */
#define MAP_BASE ( ( void * ) ( intptr_t ) ( -1ULL << 39 ) )

/** Coherent DMA mapping of the 32-bit address space */
static void *svpage_dma32_base;

/** Size of the coherent DMA mapping */
#define DMA32_LEN ( ( size_t ) 0x100000000ULL )

/**
 * Map pages
 *
 * @v phys		Physical address
 * @v len		Length
 * @v attrs		Page attributes
 * @ret virt		Mapped virtual address, or NULL on error
 */
static void * svpage_map ( physaddr_t phys, size_t len, unsigned long attrs ) {
	unsigned long satp;
	unsigned long start;
	unsigned int count;
	unsigned int stride;
	unsigned int first;
	unsigned int i;
	size_t offset;
	void *virt;

	DBGC ( &page_table, "SVPAGE mapping %#08lx+%#zx attrs %#016lx\n",
	       phys, len, attrs );

	/* Sanity checks */
	if ( ! len )
		return NULL;
	assert ( attrs & PTE_V );

	/* Use physical address directly if paging is disabled */
	__asm__ ( "csrr %0, satp" : "=r" ( satp ) );
	if ( ! satp ) {
		virt = phys_to_virt ( phys );
		DBGC ( &page_table, "SVPAGE mapped %#08lx+%#zx to %p (no "
		       "paging)\n", phys, len, virt );
		return virt;
	}

	/* Round down start address to a page boundary */
	start = ( phys & ~( MAP_PAGE_SIZE - 1 ) );
	offset = ( phys - start );
	assert ( offset < MAP_PAGE_SIZE );

	/* Calculate number of pages required */
	count = ( ( offset + len + MAP_PAGE_SIZE - 1 ) / MAP_PAGE_SIZE );
	assert ( count != 0 );
	assert ( count < ( sizeof ( page_table.pte ) /
			   sizeof ( page_table.pte[0] ) ) );

	/* Round up number of pages to a power of two */
	stride = ( 1 << ( fls ( count ) - 1 ) );
	assert ( count <= stride );

	/* Allocate pages */
	for ( first = 0 ; first < ( sizeof ( page_table.pte ) /
				    sizeof ( page_table.pte[0] ) ) ;
	      first += stride ) {

		/* Calculate virtual address */
		virt = ( MAP_BASE + ( first * MAP_PAGE_SIZE ) + offset );

		/* Check that page table entries are available */
		for ( i = first ; i < ( first + count ) ; i++ ) {
			if ( page_table.pte[i] & PTE_V ) {
				virt = NULL;
				break;
			}
		}
		if ( ! virt )
			continue;

		/* Create page table entries */
		for ( i = first ; i < ( first + count ) ; i++ ) {
			page_table.pte[i] = ( PTE_PPN ( start ) | attrs );
			start += MAP_PAGE_SIZE;
		}

		/* Mark last page as being the last in this allocation */
		page_table.pte[ i - 1 ] |= PTE_LAST;

		/* Synchronise page table updates */
		__asm__ __volatile__ ( "sfence.vma" );

		/* Return virtual address */
		DBGC ( &page_table, "SVPAGE mapped %#08lx+%#zx to %p using "
		       "PTEs [%d-%d]\n", phys, len, virt, first,
		       ( first + count - 1 ) );
		return virt;
	}

	DBGC ( &page_table, "SVPAGE could not map %#08lx+%#zx\n",
	       phys, len );
	return NULL;
}

/**
 * Unmap pages
 *
 * @v virt		Virtual address
 */
static void svpage_unmap ( const volatile void *virt ) {
	unsigned long satp;
	unsigned int first;
	unsigned int i;
	int is_last;

	DBGC ( &page_table, "SVPAGE unmapping %p\n", virt );

	/* Do nothing if paging is disabled */
	__asm__ ( "csrr %0, satp" : "=r" ( satp ) );
	if ( ! satp )
		return;

	/* Calculate first page table entry */
	first = ( ( virt - MAP_BASE ) / MAP_PAGE_SIZE );

	/* Clear page table entries */
	for ( i = first ; ; i++ ) {

		/* Sanity check */
		assert ( page_table.pte[i] & PTE_V );

		/* Check if this is the last page in this allocation */
		is_last = ( page_table.pte[i] & PTE_LAST );

		/* Clear page table entry */
		page_table.pte[i] = 0;

		/* Terminate if this was the last page */
		if ( is_last )
			break;
	}

	/* Synchronise page table updates */
	__asm__ __volatile__ ( "sfence.vma" );

	DBGC ( &page_table, "SVPAGE unmapped %p using PTEs [%d-%d]\n",
	       virt, first, i );
}

/**
 * Map pages for I/O
 *
 * @v bus_addr		Bus address
 * @v len		Length of region
 * @ret io_addr		I/O address
 */
static void * svpage_ioremap ( unsigned long bus_addr, size_t len ) {
	unsigned long attrs = ( PTE_V | PTE_R | PTE_W | PTE_A | PTE_D );
	int rc;

	/* Add Svpbmt attributes if applicable */
	if ( ( rc = hart_supported ( "_svpbmt" ) ) == 0 )
		attrs |= PTE_SVPBMT_IO;

	/* Map pages for I/O */
	return svpage_map ( bus_addr, len, attrs );
}

/**
 * Get 32-bit address space coherent DMA mapping address
 *
 * @ret base		Coherent DMA mapping base address
 */
void * svpage_dma32 ( void ) {
	unsigned long attrs = ( PTE_V | PTE_R | PTE_W | PTE_A | PTE_D );
	int rc;

	/* Add Svpbmt attributes if applicable */
	if ( ( rc = hart_supported ( "_svpbmt" ) ) == 0 )
		attrs |= PTE_SVPBMT_NC;

	/* Create mapping, if necessary */
	if ( ! svpage_dma32_base )
		svpage_dma32_base = svpage_map ( 0, DMA32_LEN, attrs );

	/* Sanity check */
	assert ( virt_to_phys ( svpage_dma32_base ) == 0 );

	return svpage_dma32_base;
}

PROVIDE_IOMAP_INLINE ( svpage, io_to_bus );
PROVIDE_IOMAP ( svpage, ioremap, svpage_ioremap );
PROVIDE_IOMAP ( svpage, iounmap, svpage_unmap );
