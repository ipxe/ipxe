#include "etherboot.h"
#include "dev.h"
#include "pci.h"

#undef DBG
#ifdef DEBUG_PCI
#define DBG(...) printf ( __VA_ARGS__ )
#else
#define DBG(...)
#endif

/*
 * Fill in parameters (vendor & device ids, class, membase etc.) for a
 * PCI device based on bus & devfn.
 *
 * Returns 1 if a device was found, 0 for no device present.
 */
static int fill_pci_device ( struct pci_device *pci ) {
	uint32_t l;
	int reg;
	
	/* Check to see if there's anything physically present.
	 */
	pci_read_config_dword ( pci, PCI_VENDOR_ID, &l );
	/* some broken boards return 0 if a slot is empty: */
	if ( ( l == 0xffffffff ) || ( l == 0x00000000 ) ) {
		return 0;
	}
	pci->vendor = l & 0xffff;
	pci->dev_id = ( l >> 16 ) & 0xffff;
	
	/* Check that we're not a duplicate function on a
	 * non-multifunction device.
	 */
	if ( PCI_FUNC ( pci->busdevfn ) != 0 ) {
		uint16_t save_busdevfn = pci->busdevfn;
		uint8_t header_type;

		pci->busdevfn &= ~PCI_FUNC ( 0xffff );
		pci_read_config_byte ( pci, PCI_HEADER_TYPE, &header_type );
		pci->busdevfn = save_busdevfn;

		if ( ! ( header_type & 0x80 ) ) {
			return 0;
		}
	}
	
	/* Get device class */
	pci_read_config_word ( pci, PCI_SUBCLASS_CODE, &pci->class );

	/* Get revision */
	pci_read_config_byte ( pci, PCI_REVISION, &pci->revision );

	/* Get the "membase" */
	pci_read_config_dword ( pci, PCI_BASE_ADDRESS_1, &pci->membase );
				
	/* Get the "ioaddr" */
	pci->ioaddr = 0;
	for ( reg = PCI_BASE_ADDRESS_0; reg <= PCI_BASE_ADDRESS_5; reg += 4 ) {
		pci_read_config_dword ( pci, reg, &pci->ioaddr );
		if ( pci->ioaddr & PCI_BASE_ADDRESS_SPACE_IO ) {
			pci->ioaddr &= PCI_BASE_ADDRESS_IO_MASK;
			if ( pci->ioaddr ) {
				break;
			}
		}
		pci->ioaddr = 0;
	}

	/* Get the irq */
	pci_read_config_byte ( pci, PCI_INTERRUPT_PIN, &pci->irq );
	if ( pci->irq ) {
		pci_read_config_byte ( pci, PCI_INTERRUPT_LINE, &pci->irq );
	}

	DBG ( "%hhx:%hhx.%d Class %hx: %hx:%hx (rev %hhx)\n",
	      PCI_BUS ( pci->busdevfn ), PCI_DEV ( pci->busdevfn ),
	      PCI_FUNC ( pci->busdevfn ), pci->class, pci->vendor, pci->dev_id,
	      pci->revision );

	return 1;
}

/*
 * Set device to be a busmaster in case BIOS neglected to do so.  Also
 * adjust PCI latency timer to a reasonable value, 32.
 */
void adjust_pci_device ( struct pci_device *pci ) {
	unsigned short	new_command, pci_command;
	unsigned char	pci_latency;

	pci_read_config_word ( pci, PCI_COMMAND, &pci_command );
	new_command = pci_command | PCI_COMMAND_MASTER | PCI_COMMAND_IO;
	if ( pci_command != new_command ) {
		DBG ( "%hhx:%hhx.%d : PCI BIOS has not enabled this device! "
		      "Updating PCI command %hX->%hX\n",
		      PCI_BUS ( pci->busdevfn ), PCI_DEV ( pci->busdevfn ),
		      PCI_FUNC ( pci->busdevfn ), pci_command, new_command );
		pci_write_config_word ( pci, PCI_COMMAND, new_command );
	}
	pci_read_config_byte ( pci, PCI_LATENCY_TIMER, &pci_latency);
	if ( pci_latency < 32 ) {
		DBG ( "%hhx:%hhx.%d : PCI latency timer (CFLT) "
		      "is unreasonably low at %d. Setting to 32 clocks.\n",
		      PCI_BUS ( pci->busdevfn ), PCI_DEV ( pci->busdevfn ),
		      PCI_FUNC ( pci->busdevfn ), pci_latency );
		pci_write_config_byte ( pci, PCI_LATENCY_TIMER, 32);
	}
}

/*
 * Obtain a struct pci * from a struct dev *
 *
 * If dev has not previously been used for a PCI device scan, blank
 * out dev.pci
 */
struct pci_device * pci_device ( struct dev *dev ) {
	struct pci_device *pci = &dev->pci;

	if ( dev->devid.bus_type != PCI_BUS_TYPE ) {
		memset ( pci, 0, sizeof ( *pci ) );
		dev->devid.bus_type = PCI_BUS_TYPE;
	}
	pci->dev = dev;
	return pci;
}

/*
 * Find a PCI device matching the specified driver
 *
 */
int find_pci_device ( struct pci_device *pci,
		      struct pci_driver *driver ) {
	int i;

	/* Iterate through all possible PCI bus:dev.fn combinations,
	 * starting where we left off.
	 */
	for ( ; pci->busdevfn <= 0xffff ; pci->busdevfn++ ) {
		/* If we've already used this device, skip it */
		if ( pci->already_tried ) {
			pci->already_tried = 0;
			continue;
		}
		
		/* Fill in device parameters, if device present */
		if ( ! fill_pci_device ( pci ) ) {
			continue;
		}
		
		/* Fix up PCI device */
		adjust_pci_device ( pci );
		
		/* Fill in dev structure, if present */
		if ( pci->dev ) {
			pci->dev->name = driver->name;
			pci->dev->devid.vendor_id = htons ( pci->vendor );
			pci->dev->devid.device_id = htons ( pci->dev_id );
		}

		/* If driver has a class, and class matches, use it */
		if ( driver->class && 
		     ( driver->class == pci->class ) ) {
			DBG ( "Driver %s matches class %hx\n",
			      driver->name, driver->class );
			pci->already_tried = 1;
			return 1;
		}
		
		/* If any of driver's IDs match, use it */
		for ( i = 0 ; i < driver->id_count; i++ ) {
			struct pci_id *id = &driver->ids[i];
			
			if ( ( pci->vendor == id->vendor ) &&
			     ( pci->dev_id == id->dev_id ) ) {
				DBG ( "Device %s (driver %s) matches "
				      "ID %hx:%hx\n", id->name, driver->name,
				      id->vendor, id->dev_id );
				if ( pci->dev )
					pci->dev->name = id->name;
				pci->already_tried = 1;
				return 1;
			}
		}

		DBG ( "No match in driver %s\n", driver->name );
	}

	/* No device found */
	return 0;
}

/*
 * Find the start of a pci resource.
 */
unsigned long pci_bar_start ( struct pci_device *pci, unsigned int index ) {
	uint32_t lo, hi;
	unsigned long bar;

	pci_read_config_dword ( pci, index, &lo );
	if ( lo & PCI_BASE_ADDRESS_SPACE_IO ) {
		bar = lo & PCI_BASE_ADDRESS_IO_MASK;
	} else {
		bar = 0;
		if ( ( lo & PCI_BASE_ADDRESS_MEM_TYPE_MASK ) ==
		     PCI_BASE_ADDRESS_MEM_TYPE_64) {
			pci_read_config_dword ( pci, index + 4, &hi );
			if ( hi ) {
#if ULONG_MAX > 0xffffffff
					bar = hi;
					bar <<= 32;
#else
					printf ( "Unhandled 64bit BAR\n" );
					return -1UL;
#endif
			}
		}
		bar |= lo & PCI_BASE_ADDRESS_MEM_MASK;
	}
	return bar + pci_bus_base ( pci );
}

/*
 * Find the size of a pci resource.
 */
unsigned long pci_bar_size ( struct pci_device *pci, unsigned int bar ) {
	uint32_t start, size;

	/* Save the original bar */
	pci_read_config_dword ( pci, bar, &start );
	/* Compute which bits can be set */
	pci_write_config_dword ( pci, bar, ~0 );
	pci_read_config_dword ( pci, bar, &size );
	/* Restore the original size */
	pci_write_config_dword ( pci, bar, start );
	/* Find the significant bits */
	if ( start & PCI_BASE_ADDRESS_SPACE_IO ) {
		size &= PCI_BASE_ADDRESS_IO_MASK;
	} else {
		size &= PCI_BASE_ADDRESS_MEM_MASK;
	}
	/* Find the lowest bit set */
	size = size & ~( size - 1 );
	return size;
}

/**
 * pci_find_capability - query for devices' capabilities 
 * @pci: PCI device to query
 * @cap: capability code
 *
 * Tell if a device supports a given PCI capability.
 * Returns the address of the requested capability structure within the
 * device's PCI configuration space or 0 in case the device does not
 * support it.  Possible values for @cap:
 *
 *  %PCI_CAP_ID_PM           Power Management 
 *
 *  %PCI_CAP_ID_AGP          Accelerated Graphics Port 
 *
 *  %PCI_CAP_ID_VPD          Vital Product Data 
 *
 *  %PCI_CAP_ID_SLOTID       Slot Identification 
 *
 *  %PCI_CAP_ID_MSI          Message Signalled Interrupts
 *
 *  %PCI_CAP_ID_CHSWP        CompactPCI HotSwap 
 */
int pci_find_capability ( struct pci_device *pci, int cap ) {
	uint16_t status;
	uint8_t pos, id;
	uint8_t hdr_type;
	int ttl = 48;

	pci_read_config_word ( pci, PCI_STATUS, &status );
	if ( ! ( status & PCI_STATUS_CAP_LIST ) )
		return 0;

	pci_read_config_byte ( pci, PCI_HEADER_TYPE, &hdr_type );
	switch ( hdr_type & 0x7F ) {
	case PCI_HEADER_TYPE_NORMAL:
	case PCI_HEADER_TYPE_BRIDGE:
	default:
		pci_read_config_byte ( pci, PCI_CAPABILITY_LIST, &pos );
		break;
	case PCI_HEADER_TYPE_CARDBUS:
		pci_read_config_byte ( pci, PCI_CB_CAPABILITY_LIST, &pos );
		break;
	}
	while ( ttl-- && pos >= 0x40 ) {
		pos &= ~3;
		pci_read_config_byte ( pci, pos + PCI_CAP_LIST_ID, &id );
		DBG ( "Capability: %d\n", id );
		if ( id == 0xff )
			break;
		if ( id == cap )
			return pos;
		pci_read_config_byte ( pci, pos + PCI_CAP_LIST_NEXT, &pos );
	}
	return 0;
}
