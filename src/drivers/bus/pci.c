#include "stdint.h"
#include "string.h"
#include "console.h"
#include "nic.h"
#include "pci.h"

/*
 * pci_io.c may know how many buses we have, in which case it can
 * overwrite this value.
 *
 */
unsigned int pci_max_bus = 0xff;

/*
 * Increment a bus_loc structure to the next possible PCI location.
 * Leave the structure zeroed and return 0 if there are no more valid
 * locations.
 *
 */
static int pci_next_location ( struct bus_loc *bus_loc ) {
	struct pci_loc *pci_loc = ( struct pci_loc * ) bus_loc;
	
	/*
	 * Ensure that there is sufficient space in the shared bus
	 * structures for a struct pci_loc and a struct
	 * pci_dev, as mandated by bus.h.
	 *
	 */
	BUS_LOC_CHECK ( struct pci_loc );
	BUS_DEV_CHECK ( struct pci_device );

	return ( ++pci_loc->busdevfn );
}

/*
 * Fill in parameters (vendor & device ids, class, membase etc.) for a
 * PCI device based on bus & devfn.
 *
 * Returns 1 if a device was found, 0 for no device present.
 *
 */
static int pci_fill_device ( struct bus_dev *bus_dev,
			     struct bus_loc *bus_loc ) {
	struct pci_loc *pci_loc = ( struct pci_loc * ) bus_loc;
	struct pci_device *pci = ( struct pci_device * ) bus_dev;
	uint16_t busdevfn = pci_loc->busdevfn;
	static struct {
		uint16_t busdevfn0;
		int is_present;
	} cache = { 0, 1 };
	uint32_t l;
	int reg;

	/* Store busdevfn in struct pci_device and set default values */
	pci->busdevfn = busdevfn;
	pci->name = "?";

	/* Check bus is within range */
	if ( PCI_BUS ( busdevfn ) > pci_max_bus ) {
		return 0;
	}

	/* Check to see if we've cached the result that this is a
	 * non-zero function on a non-existent card.  This is done to
	 * increase scan speed by a factor of 8.
	 */
	if ( ( PCI_FUNC ( busdevfn ) != 0 ) &&
	     ( PCI_FN0 ( busdevfn ) == cache.busdevfn0 ) &&
	     ( ! cache.is_present ) ) {
		return 0;
	}
	
	/* Check to see if there's anything physically present.
	 */
	pci_read_config_dword ( pci, PCI_VENDOR_ID, &l );
	/* some broken boards return 0 if a slot is empty: */
	if ( ( l == 0xffffffff ) || ( l == 0x00000000 ) ) {
		if ( PCI_FUNC ( busdevfn ) == 0 ) {
			/* Don't look for subsequent functions if the
			 * card itself is not present.
			 */
			cache.busdevfn0 = busdevfn;
			cache.is_present = 0;
		}
		return 0;
	}
	pci->vendor_id = l & 0xffff;
	pci->device_id = ( l >> 16 ) & 0xffff;
	
	/* Check that we're not a duplicate function on a
	 * non-multifunction device.
	 */
	if ( PCI_FUNC ( busdevfn ) != 0 ) {
		uint8_t header_type;

		pci->busdevfn &= PCI_FN0 ( busdevfn );
		pci_read_config_byte ( pci, PCI_HEADER_TYPE, &header_type );
		pci->busdevfn = busdevfn;

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

	DBG ( "PCI found device %hhx:%hhx.%d Class %hx: %hx:%hx (rev %hhx)\n",
	      PCI_BUS ( pci->busdevfn ), PCI_DEV ( pci->busdevfn ),
	      PCI_FUNC ( pci->busdevfn ), pci->class, pci->vendor_id,
	      pci->device_id, pci->revision );

	return 1;
}

/*
 * Test whether or not a driver is capable of driving the device.
 *
 */
static int pci_check_driver ( struct bus_dev *bus_dev,
			      struct device_driver *device_driver ) {
	struct pci_device *pci = ( struct pci_device * ) bus_dev;
	struct pci_driver *pci_driver
		= ( struct pci_driver * ) device_driver->bus_driver_info;
	unsigned int i;

	/* If driver has a class, and class matches, use it */
	if ( pci_driver->class && 
	     ( pci_driver->class == pci->class ) ) {
		DBG ( "PCI driver %s matches class %hx\n",
		      device_driver->name, pci_driver->class );
		pci->name = device_driver->name;
		return 1;
	}
		
	/* If any of driver's IDs match, use it */
	for ( i = 0 ; i < pci_driver->id_count; i++ ) {
		struct pci_id *id = &pci_driver->ids[i];
		
		if ( ( pci->vendor_id == id->vendor_id ) &&
		     ( pci->device_id == id->device_id ) ) {
			DBG ( "PCI driver %s device %s matches ID %hx:%hx\n",
			      device_driver->name, id->name,
			      id->vendor_id, id->device_id );
			pci->name = id->name;
			return 1;
		}
	}

	return 0;
}

/*
 * Describe a PCI device
 *
 */
static char * pci_describe_device ( struct bus_dev *bus_dev ) {
	struct pci_device *pci = ( struct pci_device * ) bus_dev;
	static char pci_description[] = "PCI 00:00.0";

	sprintf ( pci_description + 4, "%hhx:%hhx.%d",
		  PCI_BUS ( pci->busdevfn ), PCI_DEV ( pci->busdevfn ),
		  PCI_FUNC ( pci->busdevfn ) );
	return pci_description;
}

/*
 * Name a PCI device
 *
 */
static const char * pci_name_device ( struct bus_dev *bus_dev ) {
	struct pci_device *pci = ( struct pci_device * ) bus_dev;
	
	return pci->name;
}

/*
 * PCI bus operations table
 *
 */
struct bus_driver pci_driver __bus_driver = {
	.name			= "PCI",
	.next_location		= pci_next_location,
	.fill_device		= pci_fill_device,
	.check_driver		= pci_check_driver,
	.describe_device	= pci_describe_device,
	.name_device		= pci_name_device,
};

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
		DBG ( "PCI BIOS has not enabled device %hhx:%hhx.%d! "
		      "Updating PCI command %hX->%hX\n",
		      PCI_BUS ( pci->busdevfn ), PCI_DEV ( pci->busdevfn ),
		      PCI_FUNC ( pci->busdevfn ), pci_command, new_command );
		pci_write_config_word ( pci, PCI_COMMAND, new_command );
	}
	pci_read_config_byte ( pci, PCI_LATENCY_TIMER, &pci_latency);
	if ( pci_latency < 32 ) {
		DBG ( "PCI device %hhx:%hhx.%d latency timer is "
		      "unreasonably low at %d. Setting to 32.\n",
		      PCI_BUS ( pci->busdevfn ), PCI_DEV ( pci->busdevfn ),
		      PCI_FUNC ( pci->busdevfn ), pci_latency );
		pci_write_config_byte ( pci, PCI_LATENCY_TIMER, 32);
	}
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
		DBG ( "PCI Capability: %d\n", id );
		if ( id == 0xff )
			break;
		if ( id == cap )
			return pos;
		pci_read_config_byte ( pci, pos + PCI_CAP_LIST_NEXT, &pos );
	}
	return 0;
}

/*
 * Fill in a nic structure
 *
 */
void pci_fill_nic ( struct nic *nic, struct pci_device *pci ) {

	/* Fill in ioaddr and irqno */
	nic->ioaddr = pci->ioaddr;
	nic->irqno = pci->irq;

	/* Fill in DHCP device ID structure */
	nic->dhcp_dev_id.bus_type = PCI_BUS_TYPE;
	nic->dhcp_dev_id.vendor_id = htons ( pci->vendor_id );
	nic->dhcp_dev_id.device_id = htons ( pci->device_id );
}
