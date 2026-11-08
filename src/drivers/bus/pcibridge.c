/*
 * Copyright (C) 2022 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/pci.h>
#include <ipxe/pcibridge.h>

/** @file
 *
 * PCI-to-PCI bridge
 *
 */

/**
 * Read PCI bridge configuration
 *
 * @v bridge		PCI bridge
 */
static void pcibridge_read_config ( struct pci_bridge *bridge ) {
	struct pci_device *pci = &bridge->pci;
	uint16_t base;
	uint16_t limit;
	uint32_t base_hi;
	uint32_t limit_hi;

	/* Read bus configuration */
	pci_read_config_dword ( pci, PCI_PRIMARY, &bridge->buses );
	cpu_to_le32s ( &bridge->buses );

	/* Read memory base and limit */
	pci_read_config_word ( pci, PCI_MEM_BASE, &base );
	bridge->membase = ( ( base & ~PCI_MEM_MASK ) << 16 );
	pci_read_config_word ( pci, PCI_MEM_LIMIT, &limit );
	bridge->memlimit = ( ( ( ( limit | PCI_MEM_MASK ) + 1 ) << 16 ) - 1 );

	/* Read prefetchable memory base and limit */
	pci_read_config_word ( pci, PCI_PREFMEM_BASE, &base );
	pci_read_config_dword ( pci, PCI_PREFMEM_BASE_HI, &base_hi );
	bridge->prefmembase = ( ( ( base & ~PCI_MEM_MASK ) << 16 ) |
				( ( ( uint64_t ) base_hi ) << 32 ) );
	pci_read_config_word ( pci, PCI_PREFMEM_LIMIT, &limit );
	pci_read_config_dword ( pci, PCI_PREFMEM_LIMIT_HI, &limit_hi );
	bridge->prefmemlimit = ( ( ( ( ( limit | PCI_MEM_MASK ) + 1 ) << 16 ) |
				   ( ( ( uint64_t ) limit_hi ) << 32 ) ) - 1 );

	DBGC ( bridge, "BRIDGE " PCI_FMT " bus %02x to [%02x,%02x)\n",
	       PCI_ARGS ( pci ), bridge->primary, bridge->secondary,
	       bridge->subordinate );
	DBGC ( bridge, "BRIDGE " PCI_FMT " mem [%08x,%08x) prefmem "
	       "[%08llx,%08llx)\n", PCI_ARGS ( pci ), bridge->membase,
	       bridge->memlimit, ( ( unsigned long long ) bridge->prefmembase ),
	       ( ( unsigned long long ) bridge->prefmemlimit ) );
}

/**
 * Find bridge attached to a PCI device
 *
 * @v pci		PCI device
 * @v bridge		PCI bridge to fill in
 * @ret rc		Return status code
 */
int pcibridge_find ( struct pci_device *pci, struct pci_bridge *bridge ) {
	unsigned int bus = PCI_BUS ( pci->busdevfn );
	uint32_t busdevfn = 0;
	uint8_t hdrtype;
	int rc;

	/* Find containing bridge */
	memset ( bridge, 0, sizeof ( *bridge ) );
	do {
		/* Find next PCI device */
		if ( ( rc = pci_find_next ( &bridge->pci, &busdevfn ) ) != 0 )
			return rc;

		/* Fail if we have reached the target device's bus */
		if ( PCI_BUS ( bridge->pci.busdevfn ) >= bus )
			break;

		/* Ignore non-bridge devices */
		hdrtype = ( bridge->pci.hdrtype & PCI_HEADER_TYPE_MASK );
		if ( hdrtype != PCI_HEADER_TYPE_BRIDGE )
			continue;

		/* Read bridge configuration */
		pcibridge_read_config ( bridge );

		/* Check for matching bridge */
		if ( bridge->secondary == bus )
			return 0;

	} while ( ++busdevfn );

	DBGC ( bridge, "BRIDGE could not find bridge for " PCI_FMT "\n",
	       PCI_ARGS ( pci ) );
	return -ENODEV;
}
