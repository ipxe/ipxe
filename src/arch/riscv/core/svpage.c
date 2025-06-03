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
#include <ipxe/iomap.h>

/** @file
 *
 * Supervisor page table management
 *
 * With the 64-bit paging schemes (Sv39, Sv48, and Sv57) we choose to
 * identity-map as much as possible of the physical address space via
 * PTEs 0-255, and place a recursive page table entry in PTE 511 which
 * allows PTEs 256-510 to be used to map 2MB "megapages" within the
 * top 512MB of the 64-bit address space.  At least one of these 2MB
 * PTEs will already be in use to map iPXE itself.  The remaining PTEs
 * may be used to map I/O devices.
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

/** Page table entry address */
#define PTE_PPN( addr ) ( (addr) >> 2 )

/** The page table */
extern struct page_table page_table;

/** I/O page size
 *
 * We choose to use 2MB "megapages", since these are supported by all
 * paging levels.
 */
#define IO_PAGE_SIZE 0x200000UL

/** I/O page base address
 *
 * The recursive page table entry maps the high 1024MB of the 64-bit
 * address space as 2MB "megapages".
 */
#define IO_BASE ( ( void * ) ( intptr_t ) ( -1024 * 1024 * 1024 ) )

/**
 * Map pages for I/O
 *
 * @v bus_addr		Bus address
 * @v len		Length of region
 * @ret io_addr		I/O address
 */
static void * svpage_ioremap ( unsigned long bus_addr, size_t len ) {
	unsigned long satp;
	unsigned long start;
	unsigned int count;
	unsigned int stride;
	unsigned int first;
	unsigned int i;
	size_t offset;
	void *io_addr;

	DBGC ( &page_table, "SVPAGE mapping %#08lx+%#zx\n", bus_addr, len );

	/* Sanity check */
	if ( ! len )
		return NULL;

	/* Use physical address directly if paging is disabled */
	__asm__ ( "csrr %0, satp" : "=r" ( satp ) );
	if ( ! satp ) {
		io_addr = phys_to_virt ( bus_addr );
		DBGC ( &page_table, "SVPAGE mapped %#08lx+%#zx to %p (no "
		       "paging)\n", bus_addr, len, io_addr );
		return io_addr;
	}

	/* Round down start address to a page boundary */
	start = ( bus_addr & ~( IO_PAGE_SIZE - 1 ) );
	offset = ( bus_addr - start );
	assert ( offset < IO_PAGE_SIZE );

	/* Calculate number of pages required */
	count = ( ( offset + len + IO_PAGE_SIZE - 1 ) / IO_PAGE_SIZE );
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

		/* Calculate I/O address */
		io_addr = ( IO_BASE + ( first * IO_PAGE_SIZE ) + offset );

		/* Check that page table entries are available */
		for ( i = first ; i < ( first + count ) ; i++ ) {
			if ( page_table.pte[i] & PTE_V ) {
				io_addr = NULL;
				break;
			}
		}
		if ( ! io_addr )
			continue;

		/* Create page table entries */
		for ( i = first ; i < ( first + count ) ; i++ ) {
			page_table.pte[i] = ( PTE_PPN ( start ) | PTE_V |
					       PTE_R | PTE_W | PTE_A | PTE_D );
			start += IO_PAGE_SIZE;
		}

		/* Mark last page as being the last in this allocation */
		page_table.pte[ i - 1 ] |= PTE_LAST;

		/* Synchronise page table updates */
		__asm__ __volatile__ ( "sfence.vma" );

		/* Return I/O address */
		DBGC ( &page_table, "SVPAGE mapped %#08lx+%#zx to %p using "
		       "PTEs [%d-%d]\n", bus_addr, len, io_addr, first,
		       ( first + count - 1 ) );
		return io_addr;
	}

	DBGC ( &page_table, "SVPAGE could not map %#08lx+%#zx\n",
	       bus_addr, len );
	return NULL;
}

/**
 * Unmap pages for I/O
 *
 * @v io_addr		I/O address
 */
static void svpage_iounmap ( volatile const void *io_addr ) {
	unsigned long satp;
	unsigned int first;
	unsigned int i;
	int is_last;

	DBGC ( &page_table, "SVPAGE unmapping %p\n", io_addr );

	/* Do nothing if paging is disabled */
	__asm__ ( "csrr %0, satp" : "=r" ( satp ) );
	if ( ! satp )
		return;

	/* Calculate first page table entry */
	first = ( ( io_addr - IO_BASE ) / IO_PAGE_SIZE );

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
	       io_addr, first, i );
}

PROVIDE_IOMAP_INLINE ( svpage, io_to_bus );
PROVIDE_IOMAP ( svpage, ioremap, svpage_ioremap );
PROVIDE_IOMAP ( svpage, iounmap, svpage_iounmap );
