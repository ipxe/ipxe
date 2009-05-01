/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * Based in part on pci.c from Etherboot 5.4, by Ken Yap and David
 * Munro, in turn based on the Linux kernel's PCI implementation.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <gpxe/tables.h>
#include <gpxe/device.h>
#include <gpxe/pci.h>

/** @file
 *
 * PCI bus
 *
 */

static void pcibus_remove ( struct root_device *rootdev );

/**
 * Read PCI BAR
 *
 * @v pci		PCI device
 * @v reg		PCI register number
 * @ret bar		Base address register
 *
 * Reads the specified PCI base address register, including the flags
 * portion.  64-bit BARs will be handled automatically.  If the value
 * of the 64-bit BAR exceeds the size of an unsigned long (i.e. if the
 * high dword is non-zero on a 32-bit platform), then the value
 * returned will be zero plus the flags for a 64-bit BAR.  Unreachable
 * 64-bit BARs are therefore returned as uninitialised 64-bit BARs.
 */
static unsigned long pci_bar ( struct pci_device *pci, unsigned int reg ) {
	uint32_t low;
	uint32_t high;

	pci_read_config_dword ( pci, reg, &low );
	if ( ( low & (PCI_BASE_ADDRESS_SPACE|PCI_BASE_ADDRESS_MEM_TYPE_MASK) )
	     == (PCI_BASE_ADDRESS_SPACE_MEMORY|PCI_BASE_ADDRESS_MEM_TYPE_64) ){
		pci_read_config_dword ( pci, reg + 4, &high );
		if ( high ) {
			if ( sizeof ( unsigned long ) > sizeof ( uint32_t ) ) {
				return ( ( ( uint64_t ) high << 32 ) | low );
			} else {
				DBG ( "Unhandled 64-bit BAR %08x%08x\n",
				      high, low );
				return PCI_BASE_ADDRESS_MEM_TYPE_64;
			}
		}
	}
	return low;
}

/**
 * Find the start of a PCI BAR
 *
 * @v pci		PCI device
 * @v reg		PCI register number
 * @ret start		BAR start address
 *
 * Reads the specified PCI base address register, and returns the
 * address portion of the BAR (i.e. without the flags).
 *
 * If the address exceeds the size of an unsigned long (i.e. if a
 * 64-bit BAR has a non-zero high dword on a 32-bit machine), the
 * return value will be zero.
 */
unsigned long pci_bar_start ( struct pci_device *pci, unsigned int reg ) {
	unsigned long bar;

	bar = pci_bar ( pci, reg );
	if ( (bar & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_MEMORY ){
		return ( bar & PCI_BASE_ADDRESS_MEM_MASK );
	} else {
		return ( bar & PCI_BASE_ADDRESS_IO_MASK );
	}
}

/**
 * Read membase and ioaddr for a PCI device
 *
 * @v pci		PCI device
 *
 * This scans through all PCI BARs on the specified device.  The first
 * valid memory BAR is recorded as pci_device::membase, and the first
 * valid IO BAR is recorded as pci_device::ioaddr.
 *
 * 64-bit BARs are handled automatically.  On a 32-bit platform, if a
 * 64-bit BAR has a non-zero high dword, it will be regarded as
 * invalid.
 */
static void pci_read_bases ( struct pci_device *pci ) {
	unsigned long bar;
	int reg;

	for ( reg = PCI_BASE_ADDRESS_0; reg <= PCI_BASE_ADDRESS_5; reg += 4 ) {
		bar = pci_bar ( pci, reg );
		if ( bar & PCI_BASE_ADDRESS_SPACE_IO ) {
			if ( ! pci->ioaddr )
				pci->ioaddr = 
					( bar & PCI_BASE_ADDRESS_IO_MASK );
		} else {
			if ( ! pci->membase )
				pci->membase =
					( bar & PCI_BASE_ADDRESS_MEM_MASK );
			/* Skip next BAR if 64-bit */
			if ( bar & PCI_BASE_ADDRESS_MEM_TYPE_64 )
				reg += 4;
		}
	}
}

/**
 * Enable PCI device
 *
 * @v pci		PCI device
 *
 * Set device to be a busmaster in case BIOS neglected to do so.  Also
 * adjust PCI latency timer to a reasonable value, 32.
 */
void adjust_pci_device ( struct pci_device *pci ) {
	unsigned short new_command, pci_command;
	unsigned char pci_latency;

	pci_read_config_word ( pci, PCI_COMMAND, &pci_command );
	new_command = ( pci_command | PCI_COMMAND_MASTER |
			PCI_COMMAND_MEM | PCI_COMMAND_IO );
	if ( pci_command != new_command ) {
		DBG ( "PCI BIOS has not enabled device %02x:%02x.%x! "
		      "Updating PCI command %04x->%04x\n", pci->bus,
		      PCI_SLOT ( pci->devfn ), PCI_FUNC ( pci->devfn ),
		      pci_command, new_command );
		pci_write_config_word ( pci, PCI_COMMAND, new_command );
	}

	pci_read_config_byte ( pci, PCI_LATENCY_TIMER, &pci_latency);
	if ( pci_latency < 32 ) {
		DBG ( "PCI device %02x:%02x.%x latency timer is unreasonably "
		      "low at %d. Setting to 32.\n", pci->bus,
		      PCI_SLOT ( pci->devfn ), PCI_FUNC ( pci->devfn ),
		      pci_latency );
		pci_write_config_byte ( pci, PCI_LATENCY_TIMER, 32);
	}
}

/**
 * Probe a PCI device
 *
 * @v pci		PCI device
 * @ret rc		Return status code
 *
 * Searches for a driver for the PCI device.  If a driver is found,
 * its probe() routine is called.
 */
static int pci_probe ( struct pci_device *pci ) {
	struct pci_driver *driver;
	struct pci_device_id *id;
	unsigned int i;
	int rc;

	DBG ( "Adding PCI device %02x:%02x.%x (%04x:%04x mem %lx io %lx "
	      "irq %d)\n", pci->bus, PCI_SLOT ( pci->devfn ),
	      PCI_FUNC ( pci->devfn ), pci->vendor, pci->device,
	      pci->membase, pci->ioaddr, pci->irq );

	for_each_table_entry ( driver, PCI_DRIVERS ) {
		for ( i = 0 ; i < driver->id_count ; i++ ) {
			id = &driver->ids[i];
			if ( ( id->vendor != PCI_ANY_ID ) &&
			     ( id->vendor != pci->vendor ) )
				continue;
			if ( ( id->device != PCI_ANY_ID ) &&
			     ( id->device != pci->device ) )
				continue;
			pci->driver = driver;
			pci->driver_name = id->name;
			DBG ( "...using driver %s\n", pci->driver_name );
			if ( ( rc = driver->probe ( pci, id ) ) != 0 ) {
				DBG ( "......probe failed\n" );
				continue;
			}
			return 0;
		}
	}

	DBG ( "...no driver found\n" );
	return -ENOTTY;
}

/**
 * Remove a PCI device
 *
 * @v pci		PCI device
 */
static void pci_remove ( struct pci_device *pci ) {
	pci->driver->remove ( pci );
	DBG ( "Removed PCI device %02x:%02x.%x\n", pci->bus,
	      PCI_SLOT ( pci->devfn ), PCI_FUNC ( pci->devfn ) );
}

/**
 * Probe PCI root bus
 *
 * @v rootdev		PCI bus root device
 *
 * Scans the PCI bus for devices and registers all devices it can
 * find.
 */
static int pcibus_probe ( struct root_device *rootdev ) {
	struct pci_device *pci = NULL;
	unsigned int max_bus;
	unsigned int bus;
	unsigned int devfn;
	uint8_t hdrtype = 0;
	uint32_t tmp;
	int rc;

	max_bus = pci_max_bus();
	for ( bus = 0 ; bus <= max_bus ; bus++ ) {
		for ( devfn = 0 ; devfn <= 0xff ; devfn++ ) {

			/* Allocate struct pci_device */
			if ( ! pci )
				pci = malloc ( sizeof ( *pci ) );
			if ( ! pci ) {
				rc = -ENOMEM;
				goto err;
			}
			memset ( pci, 0, sizeof ( *pci ) );
			pci->bus = bus;
			pci->devfn = devfn;
			
			/* Skip all but the first function on
			 * non-multifunction cards
			 */
			if ( PCI_FUNC ( devfn ) == 0 ) {
				pci_read_config_byte ( pci, PCI_HEADER_TYPE,
						       &hdrtype );
			} else if ( ! ( hdrtype & 0x80 ) ) {
					continue;
			}

			/* Check for physical device presence */
			pci_read_config_dword ( pci, PCI_VENDOR_ID, &tmp );
			if ( ( tmp == 0xffffffff ) || ( tmp == 0 ) )
				continue;
			
			/* Populate struct pci_device */
			pci->vendor = ( tmp & 0xffff );
			pci->device = ( tmp >> 16 );
			pci_read_config_dword ( pci, PCI_REVISION, &tmp );
			pci->class = ( tmp >> 8 );
			pci_read_config_byte ( pci, PCI_INTERRUPT_LINE,
					       &pci->irq );
			pci_read_bases ( pci );

			/* Add to device hierarchy */
			snprintf ( pci->dev.name, sizeof ( pci->dev.name ),
				   "PCI%02x:%02x.%x", bus,
				   PCI_SLOT ( devfn ), PCI_FUNC ( devfn ) );
			pci->dev.desc.bus_type = BUS_TYPE_PCI;
			pci->dev.desc.location = PCI_BUSDEVFN (bus, devfn);
			pci->dev.desc.vendor = pci->vendor;
			pci->dev.desc.device = pci->device;
			pci->dev.desc.class = pci->class;
			pci->dev.desc.ioaddr = pci->ioaddr;
			pci->dev.desc.irq = pci->irq;
			pci->dev.parent = &rootdev->dev;
			list_add ( &pci->dev.siblings, &rootdev->dev.children);
			INIT_LIST_HEAD ( &pci->dev.children );
			
			/* Look for a driver */
			if ( pci_probe ( pci ) == 0 ) {
				/* pcidev registered, we can drop our ref */
				pci = NULL;
			} else {
				/* Not registered; re-use struct pci_device */
				list_del ( &pci->dev.siblings );
			}
		}
	}

	free ( pci );
	return 0;

 err:
	free ( pci );
	pcibus_remove ( rootdev );
	return rc;
}

/**
 * Remove PCI root bus
 *
 * @v rootdev		PCI bus root device
 */
static void pcibus_remove ( struct root_device *rootdev ) {
	struct pci_device *pci;
	struct pci_device *tmp;

	list_for_each_entry_safe ( pci, tmp, &rootdev->dev.children,
				   dev.siblings ) {
		pci_remove ( pci );
		list_del ( &pci->dev.siblings );
		free ( pci );
	}
}

/** PCI bus root device driver */
static struct root_driver pci_root_driver = {
	.probe = pcibus_probe,
	.remove = pcibus_remove,
};

/** PCI bus root device */
struct root_device pci_root_device __root_device = {
	.dev = { .name = "PCI" },
	.driver = &pci_root_driver,
};
