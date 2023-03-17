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
#include <errno.h>
#include <byteswap.h>
#include <ipxe/pci.h>
#include <ipxe/pcibridge.h>

/** @file
 *
 * PCI-to-PCI bridge
 *
 */

/** List of all PCI bridges */
static LIST_HEAD ( pcibridges );

/**
 * Find bridge attached to a PCI device
 *
 * @v pci		PCI device
 * @ret bridge		PCI bridge, or NULL
 */
struct pci_bridge * pcibridge_find ( struct pci_device *pci ) {
	unsigned int bus = PCI_BUS ( pci->busdevfn );
	struct pci_bridge *bridge;

	/* Find matching bridge */
	list_for_each_entry ( bridge, &pcibridges, list ) {
		if ( bus == bridge->secondary )
			return bridge;
	}

	return NULL;
}

/**
 * Probe PCI device
 *
 * @v pci		PCI device
 * @ret rc		Return status code
 */
static int pcibridge_probe ( struct pci_device *pci ) {
	struct pci_bridge *bridge;
	uint16_t base;
	uint16_t limit;
	int rc;

	/* Allocate and initialise structure */
	bridge = zalloc ( sizeof ( *bridge ) );
	if ( ! bridge ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	bridge->pci = pci;

	/* Read configuration */
	pci_read_config_dword ( pci, PCI_PRIMARY, &bridge->buses );
	cpu_to_le32s ( &buses );
	pci_read_config_word ( pci, PCI_MEM_BASE, &base );
	bridge->membase = ( ( base & ~PCI_MEM_MASK ) << 16 );
	pci_read_config_word ( pci, PCI_MEM_LIMIT, &limit );
	bridge->memlimit = ( ( ( ( limit | PCI_MEM_MASK ) + 1 ) << 16 ) - 1 );
	DBGC ( bridge, "BRIDGE " PCI_FMT " bus %02x to [%02x,%02x) mem "
	       "[%08x,%08x)\n", PCI_ARGS ( pci ), bridge->primary,
	       bridge->secondary, bridge->subordinate, bridge->membase,
	       bridge->memlimit );

	/* Add to list of PCI bridges */
	list_add ( &bridge->list, &pcibridges );

	pci_set_drvdata ( pci, bridge );
	return 0;

	free ( bridge );
 err_alloc:
	return rc;
}

/**
 * Remove PCI device
 *
 * @v pci		PCI device
 */
static void pcibridge_remove ( struct pci_device *pci ) {
	struct pci_bridge *bridge = pci_get_drvdata ( pci );

	/* Remove from list of bridges */
	list_del ( &bridge->list );

	/* Free device */
	free ( bridge );
}

/** Bridge PCI device IDs */
static struct pci_device_id pcibridge_ids[] = {
	PCI_ROM ( 0xffff, 0xffff, "bridge", "Bridge", 0 ),
};

/** Bridge PCI driver */
struct pci_driver pcibridge_driver __pci_driver = {
	.ids = pcibridge_ids,
	.id_count = ( sizeof ( pcibridge_ids ) / sizeof ( pcibridge_ids[0] ) ),
	.class = PCI_CLASS_ID ( PCI_CLASS_BRIDGE, PCI_CLASS_BRIDGE_PCI,
				PCI_ANY_ID ),
	.probe = pcibridge_probe,
	.remove = pcibridge_remove,
};
