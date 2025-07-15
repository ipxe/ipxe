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
#include <ipxe/init.h>
#include <ipxe/pci.h>
#include <ipxe/ecam.h>
#include <ipxe/pcibios.h>
#include <ipxe/pcidirect.h>
#include <ipxe/pcicloud.h>

/** @file
 *
 * Cloud VM PCI configuration space access
 *
 */

/** Selected PCI configuration space access API */
static struct pci_api *pcicloud = &ecam_api;

/**
 * Find next PCI bus:dev.fn address range in system
 *
 * @v busdevfn		Starting PCI bus:dev.fn address
 * @v range		PCI bus:dev.fn address range to fill in
 */
static void pcicloud_discover ( uint32_t busdevfn, struct pci_range *range ) {

	pcicloud->pci_discover ( busdevfn, range );
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

	return pcicloud->pci_read_config_byte ( pci, where, value );
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

	return pcicloud->pci_read_config_word ( pci, where, value );
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

	return pcicloud->pci_read_config_dword ( pci, where, value );
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

	return pcicloud->pci_write_config_byte ( pci, where, value );
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

	return pcicloud->pci_write_config_word ( pci, where, value );
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

	return pcicloud->pci_write_config_dword ( pci, where, value );
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

	return pcicloud->pci_ioremap ( pci, bus_addr, len );
}

PROVIDE_PCIAPI_INLINE ( cloud, pci_can_probe );
PROVIDE_PCIAPI ( cloud, pci_discover, pcicloud_discover );
PROVIDE_PCIAPI ( cloud, pci_read_config_byte, pcicloud_read_config_byte );
PROVIDE_PCIAPI ( cloud, pci_read_config_word, pcicloud_read_config_word );
PROVIDE_PCIAPI ( cloud, pci_read_config_dword, pcicloud_read_config_dword );
PROVIDE_PCIAPI ( cloud, pci_write_config_byte, pcicloud_write_config_byte );
PROVIDE_PCIAPI ( cloud, pci_write_config_word, pcicloud_write_config_word );
PROVIDE_PCIAPI ( cloud, pci_write_config_dword, pcicloud_write_config_dword );
PROVIDE_PCIAPI ( cloud, pci_ioremap, pcicloud_ioremap );

/**
 * Initialise cloud VM PCI configuration space access
 *
 */
static void pcicloud_init ( void ) {
	static struct pci_api *apis[] = {
		&ecam_api, &pcibios_api, &pcidirect_api
	};
	struct pci_device pci;
	uint32_t busdevfn;
	unsigned int i;
	int rc;

	/* Select first API that successfully discovers a PCI device */
	for ( i = 0 ; i < ( sizeof ( apis ) / sizeof ( apis[0] ) ) ; i++ ) {
		pcicloud = apis[i];
		busdevfn = 0;
		if ( ( rc = pci_find_next ( &pci, &busdevfn ) ) == 0 ) {
			DBGC ( pcicloud, "PCICLOUD selected %s API (found "
			       PCI_FMT ")\n", pcicloud->name,
			       PCI_ARGS ( &pci ) );
			return;
		}
	}

	/* Fall back to using final attempted API if no devices found */
	pcicloud = apis[ i - 1 ];
	DBGC ( pcicloud, "PCICLOUD selected %s API (nothing detected)\n",
	       pcicloud->name );
}

/** Cloud VM PCI configuration space access initialisation function */
struct init_fn pcicloud_init_fn __init_fn ( INIT_EARLY ) = {
	.name = "pcicloud",
	.initialise = pcicloud_init,
};
