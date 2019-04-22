/*
 * Copyright (C) 2019 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <errno.h>
#include <assert.h>
#include <ipxe/pci.h>
#include <ipxe/pcimsix.h>

/** @file
 *
 * PCI MSI-X interrupts
 *
 */

/**
 * Get MSI-X descriptor name (for debugging)
 *
 * @v cfg		Configuration space offset
 * @ret name		Descriptor name
 */
static const char * pci_msix_name ( unsigned int cfg ) {

	switch ( cfg ) {
	case PCI_MSIX_DESC_TABLE:	return "table";
	case PCI_MSIX_DESC_PBA:		return "PBA";
	default:			return "<UNKNOWN>";
	}
}

/**
 * Map MSI-X BAR portion
 *
 * @v pci		PCI device
 * @v msix		MSI-X capability
 * @v cfg		Configuration space offset
 * @ret io		I/O address
 */
static void * pci_msix_ioremap ( struct pci_device *pci, struct pci_msix *msix,
				 unsigned int cfg ) {
	uint32_t desc;
	unsigned int bar;
	unsigned long start;
	unsigned long offset;
	unsigned long base;
	void *io;

	/* Read descriptor */
	pci_read_config_dword ( pci, ( msix->cap + cfg ), &desc );

	/* Get BAR */
	bar = PCI_MSIX_DESC_BIR ( desc );
	offset = PCI_MSIX_DESC_OFFSET ( desc );
	start = pci_bar_start ( pci, PCI_BASE_ADDRESS ( bar ) );
	if ( ! start ) {
		DBGC ( msix, "MSI-X %p %s could not find BAR%d\n",
		       msix, pci_msix_name ( cfg ), bar );
		return NULL;
	}
	base = ( start + offset );
	DBGC ( msix, "MSI-X %p %s at %#08lx (BAR%d+%#lx)\n",
	       msix, pci_msix_name ( cfg ), base, bar, offset );

	/* Map BAR portion */
	io = ioremap ( ( start + offset ), PCI_MSIX_LEN );
	if ( ! io ) {
		DBGC ( msix, "MSI-X %p %s could not map %#08lx\n",
		       msix, pci_msix_name ( cfg ), base );
		return NULL;
	}

	return io;
}

/**
 * Enable MSI-X interrupts
 *
 * @v pci		PCI device
 * @v msix		MSI-X capability
 * @ret rc		Return status code
 */
int pci_msix_enable ( struct pci_device *pci, struct pci_msix *msix ) {
	uint16_t ctrl;
	int rc;

	/* Locate capability */
	msix->cap = pci_find_capability ( pci, PCI_CAP_ID_MSIX );
	if ( ! msix->cap ) {
		DBGC ( msix, "MSI-X %p found no MSI-X capability in "
		       PCI_FMT "\n", msix, PCI_ARGS ( pci ) );
		rc = -ENOENT;
		goto err_cap;
	}

	/* Extract interrupt count */
	pci_read_config_word ( pci, ( msix->cap + PCI_MSIX_CTRL ), &ctrl );
	msix->count = ( PCI_MSIX_CTRL_SIZE ( ctrl ) + 1 );
	DBGC ( msix, "MSI-X %p has %d vectors for " PCI_FMT "\n",
	       msix, msix->count, PCI_ARGS ( pci ) );

	/* Map MSI-X table */
	msix->table = pci_msix_ioremap ( pci, msix, PCI_MSIX_DESC_TABLE );
	if ( ! msix->table ) {
		rc = -ENOENT;
		goto err_table;
	}

	/* Map pending bit array */
	msix->pba = pci_msix_ioremap ( pci, msix, PCI_MSIX_DESC_PBA );
	if ( ! msix->pba ) {
		rc = -ENOENT;
		goto err_pba;
	}

	/* Enable MSI-X */
	ctrl &= ~PCI_MSIX_CTRL_MASK;
	ctrl |= PCI_MSIX_CTRL_ENABLE;
	pci_write_config_word ( pci, ( msix->cap + PCI_MSIX_CTRL ), ctrl );

	return 0;

	iounmap ( msix->pba );
 err_pba:
	iounmap ( msix->table );
 err_table:
 err_cap:
	return rc;
}

/**
 * Disable MSI-X interrupts
 *
 * @v pci		PCI device
 * @v msix		MSI-X capability
 */
void pci_msix_disable ( struct pci_device *pci, struct pci_msix *msix ) {
	uint16_t ctrl;

	/* Disable MSI-X */
	pci_read_config_word ( pci, ( msix->cap + PCI_MSIX_CTRL ), &ctrl );
	ctrl &= ~PCI_MSIX_CTRL_ENABLE;
	pci_write_config_word ( pci, ( msix->cap + PCI_MSIX_CTRL ), ctrl );

	/* Unmap pending bit array */
	iounmap ( msix->pba );

	/* Unmap MSI-X table */
	iounmap ( msix->table );
}

/**
 * Map MSI-X interrupt vector
 *
 * @v msix		MSI-X capability
 * @v vector		MSI-X vector
 * @v address		Message address
 * @v data		Message data
 */
void pci_msix_map ( struct pci_msix *msix, unsigned int vector,
		    physaddr_t address, uint32_t data ) {
	void *base;

	/* Sanity check */
	assert ( vector < msix->count );

	/* Map interrupt vector */
	base = ( msix->table + PCI_MSIX_VECTOR ( vector ) );
	writel ( ( address & 0xffffffffUL ), ( base + PCI_MSIX_ADDRESS_LO ) );
	if ( sizeof ( address ) > sizeof ( uint32_t ) ) {
		writel ( ( ( ( uint64_t ) address ) >> 32 ),
			 ( base + PCI_MSIX_ADDRESS_HI ) );
	} else {
		writel ( 0, ( base + PCI_MSIX_ADDRESS_HI ) );
	}
	writel ( data, ( base + PCI_MSIX_DATA ) );
}

/**
 * Control MSI-X interrupt vector
 *
 * @v msix		MSI-X capability
 * @v vector		MSI-X vector
 * @v mask		Control mask
 */
void pci_msix_control ( struct pci_msix *msix, unsigned int vector,
			uint32_t mask ) {
	void *base;
	uint32_t ctrl;

	/* Mask/unmask interrupt vector */
	base = ( msix->table + PCI_MSIX_VECTOR ( vector ) );
	ctrl = readl ( base + PCI_MSIX_CONTROL );
	ctrl &= ~PCI_MSIX_CONTROL_MASK;
	ctrl |= mask;
	writel ( ctrl, ( base + PCI_MSIX_CONTROL ) );
}

/**
 * Dump MSI-X interrupt state (for debugging)
 *
 * @v msix		MSI-X capability
 * @v vector		MSI-X vector
 */
void pci_msix_dump ( struct pci_msix *msix, unsigned int vector ) {
	void *base;
	uint32_t address_hi;
	uint32_t address_lo;
	physaddr_t address;
	uint32_t data;
	uint32_t ctrl;
	uint32_t pba;

	/* Do nothing in non-debug builds */
	if ( ! DBG_LOG )
		return;

	/* Mask/unmask interrupt vector */
	base = ( msix->table + PCI_MSIX_VECTOR ( vector ) );
	address_hi = readl ( base + PCI_MSIX_ADDRESS_HI );
	address_lo = readl ( base + PCI_MSIX_ADDRESS_LO );
	data = readl ( base + PCI_MSIX_DATA );
	ctrl = readl ( base + PCI_MSIX_CONTROL );
	pba = readl ( msix->pba );
	address = ( ( ( ( uint64_t ) address_hi ) << 32 ) | address_lo );
	DBGC ( msix, "MSI-X %p vector %d %#08x => %#08lx%s%s\n",
	       msix, vector, data, address,
	       ( ( ctrl & PCI_MSIX_CONTROL_MASK ) ? " (masked)" : "" ),
	       ( ( pba & ( 1 << vector ) ) ? " (pending)" : "" ) );
}
