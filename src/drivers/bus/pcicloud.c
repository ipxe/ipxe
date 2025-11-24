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

#include <stdint.h>
#include <string.h>
#include <ipxe/pci.h>
#include <ipxe/pcicloud.h>

/** @file
 *
 * Cloud VM PCI configuration space access
 *
 */

/** Cached PCI configuration space access API */
static struct {
	/** PCI bus:dev.fn address range */
	struct pci_range range;
	/** API for this bus:dev.fn address */
	struct pci_api *api;
} pcicloud;

/**
 * Find PCI configuration space access API for address
 *
 * @v busdevfn		Starting PCI bus:dev.fn address
 * @v range		PCI bus:dev.fn address range to fill in
 * @ret api		Configuration space access API, or NULL
 */
static struct pci_api * pcicloud_find ( uint32_t busdevfn,
					struct pci_range *range ) {
	struct pci_range candidate;
	struct pci_api *api;
	uint32_t best = 0;
	uint32_t index;
	uint32_t first;
	uint32_t last;

	/* Return empty range on error */
	range->count = 0;

	/* Try discovery via all known APIs */
	for_each_table_entry ( api, PCI_APIS ) {

		/* Discover via this API */
		api->pci_discover ( busdevfn, &candidate );

		/* Check for a matching or new closest allocation */
		index = ( busdevfn - candidate.start );
		if ( ( index < candidate.count ) || ( index > best ) ) {
			memcpy ( range, &candidate, sizeof ( *range ) );
			best = index;
		}

		/* Stop if this range contains the target bus:dev.fn address */
		if ( index < candidate.count ) {
			first = range->start;
			last = ( range->start + range->count - 1 );
			DBGC ( &pcicloud, "PCICLOUD [" PCI_FMT "," PCI_FMT ") "
			       "using %s API\n", PCI_SEG ( first ),
			       PCI_BUS ( first ), PCI_SLOT ( first ),
			       PCI_FUNC ( first ), PCI_SEG ( last ),
			       PCI_BUS ( last ), PCI_SLOT ( last ),
			       PCI_FUNC ( last ), api->name );
			return api;
		}
	}

	return NULL;
}

/**
 * Find next PCI bus:dev.fn address range in system
 *
 * @v busdevfn		Starting PCI bus:dev.fn address
 * @v range		PCI bus:dev.fn address range to fill in
 */
static void pcicloud_discover ( uint32_t busdevfn, struct pci_range *range ) {

	/* Find new range, if any */
	pcicloud_find ( busdevfn, range );
}

/**
 * Find configuration space access API for PCI device
 *
 * @v pci		PCI device
 * @ret api		Configuration space access API
 */
static struct pci_api * pcicloud_api ( struct pci_device *pci ) {
	struct pci_range *range = &pcicloud.range;
	struct pci_api *api;
	uint32_t first;
	uint32_t last;

	/* Reuse cached API if applicable */
	if ( ( pci->busdevfn - range->start ) < range->count )
		return pcicloud.api;

	/* Find highest priority API claiming this range */
	api = pcicloud_find ( pci->busdevfn, range );

	/* Fall back to lowest priority API for any unclaimed gaps in ranges */
	if ( ! api ) {
		api = ( table_end ( PCI_APIS ) - 1 );
		range->count = ( range->start - pci->busdevfn );
		range->start = pci->busdevfn;
		first = range->start;
		last = ( range->start + range->count - 1 );
		DBGC ( &pcicloud, "PCICLOUD [" PCI_FMT "," PCI_FMT ") falling "
		       "back to %s API\n", PCI_SEG ( first ),
		       PCI_BUS ( first ), PCI_SLOT ( first ),
		       PCI_FUNC ( first ), PCI_SEG ( last ), PCI_BUS ( last ),
		       PCI_SLOT ( last ),  PCI_FUNC ( last ), api->name );
	}

	/* Cache API for this range */
	pcicloud.api = api;

	return api;
}

/**
 * Check if PCI bus probing is allowed
 *
 * @v pci		PCI device
 * @ret ok		Bus probing is allowed
 */
static int pcicloud_can_probe ( struct pci_device *pci ) {
	struct pci_api *api = pcicloud_api ( pci );

	return api->pci_can_probe ( pci );
}

/**
 * Read byte from PCI configuration space
 *
 * @v pci		PCI device
 * @v where		Location within PCI configuration space
 * @v value		Value read
 * @ret rc		Return status code
 */
static int pcicloud_read_config_byte ( struct pci_device *pci,
				       unsigned int where, uint8_t *value ) {
	struct pci_api *api = pcicloud_api ( pci );

	return api->pci_read_config_byte ( pci, where, value );
}

/**
 * Read 16-bit word from PCI configuration space
 *
 * @v pci		PCI device
 * @v where		Location within PCI configuration space
 * @v value		Value read
 * @ret rc		Return status code
 */
static int pcicloud_read_config_word ( struct pci_device *pci,
				       unsigned int where, uint16_t *value ) {
	struct pci_api *api = pcicloud_api ( pci );

	return api->pci_read_config_word ( pci, where, value );
}

/**
 * Read 32-bit dword from PCI configuration space
 *
 * @v pci		PCI device
 * @v where		Location within PCI configuration space
 * @v value		Value read
 * @ret rc		Return status code
 */
static int pcicloud_read_config_dword ( struct pci_device *pci,
					unsigned int where, uint32_t *value ) {
	struct pci_api *api = pcicloud_api ( pci );

	return api->pci_read_config_dword ( pci, where, value );
}

/**
 * Write byte to PCI configuration space
 *
 * @v pci		PCI device
 * @v where		Location within PCI configuration space
 * @v value		Value to be written
 * @ret rc		Return status code
 */
static int pcicloud_write_config_byte ( struct pci_device *pci,
					unsigned int where, uint8_t value ) {
	struct pci_api *api = pcicloud_api ( pci );

	return api->pci_write_config_byte ( pci, where, value );
}

/**
 * Write 16-bit word to PCI configuration space
 *
 * @v pci		PCI device
 * @v where		Location within PCI configuration space
 * @v value		Value to be written
 * @ret rc		Return status code
 */
static int pcicloud_write_config_word ( struct pci_device *pci,
					unsigned int where, uint16_t value ) {
	struct pci_api *api = pcicloud_api ( pci );

	return api->pci_write_config_word ( pci, where, value );
}

/**
 * Write 32-bit dword to PCI configuration space
 *
 * @v pci		PCI device
 * @v where		Location within PCI configuration space
 * @v value		Value to be written
 * @ret rc		Return status code
 */
static int pcicloud_write_config_dword ( struct pci_device *pci,
					 unsigned int where, uint32_t value ) {
	struct pci_api *api = pcicloud_api ( pci );

	return api->pci_write_config_dword ( pci, where, value );
}

/**
 * Map PCI bus address as an I/O address
 *
 * @v bus_addr		PCI bus address
 * @v len		Length of region
 * @ret io_addr		I/O address, or NULL on error
 */
static void * pcicloud_ioremap ( struct pci_device *pci,
				 unsigned long bus_addr, size_t len ) {
	struct pci_api *api = pcicloud_api ( pci );

	return api->pci_ioremap ( pci, bus_addr, len );
}

PROVIDE_PCIAPI ( cloud, pci_can_probe, pcicloud_can_probe );
PROVIDE_PCIAPI ( cloud, pci_discover, pcicloud_discover );
PROVIDE_PCIAPI ( cloud, pci_read_config_byte, pcicloud_read_config_byte );
PROVIDE_PCIAPI ( cloud, pci_read_config_word, pcicloud_read_config_word );
PROVIDE_PCIAPI ( cloud, pci_read_config_dword, pcicloud_read_config_dword );
PROVIDE_PCIAPI ( cloud, pci_write_config_byte, pcicloud_write_config_byte );
PROVIDE_PCIAPI ( cloud, pci_write_config_word, pcicloud_write_config_word );
PROVIDE_PCIAPI ( cloud, pci_write_config_dword, pcicloud_write_config_dword );
PROVIDE_PCIAPI ( cloud, pci_ioremap, pcicloud_ioremap );
