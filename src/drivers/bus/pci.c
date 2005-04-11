#include "etherboot.h"
#include "pci.h"

#undef DBG
#ifdef DEBUG_PCI
#define DBG(...) printf ( __VA_ARGS__ )
#else
#define DBG(...)
#endif


/*
 * Set device to be a busmaster in case BIOS neglected to do so.  Also
 * adjust PCI latency timer to a reasonable value, 32.
 */
void adjust_pci_device ( struct pci_device *dev ) {
	unsigned short	new_command, pci_command;
	unsigned char	pci_latency;

	pci_read_config_word ( dev, PCI_COMMAND, &pci_command );
	new_command = pci_command | PCI_COMMAND_MASTER | PCI_COMMAND_IO;
	if ( pci_command != new_command ) {
		DBG ( "The PCI BIOS has not enabled this device!\n"
		      "Updating PCI command %hX->%hX. bus %hhX dev_fn %hhX\n",
		      pci_command, new_command, p->bus, p->devfn );
		pci_write_config_word ( dev, PCI_COMMAND, new_command );
	}
	pci_read_config_byte ( dev, PCI_LATENCY_TIMER, &pci_latency);
	if ( pci_latency < 32 ) {
		DBG ( "PCI latency timer (CFLT) is unreasonably low at %d. "
		      "Setting to 32 clocks.\n", pci_latency );
		pci_write_config_byte ( dev, PCI_LATENCY_TIMER, 32);
	}
}

/*
 * Find the start of a pci resource.
 */
unsigned long pci_bar_start ( struct pci_device *dev, unsigned int index ) {
	uint32_t lo, hi;
	unsigned long bar;

	pci_read_config_dword ( dev, index, &lo );
	if ( lo & PCI_BASE_ADDRESS_SPACE_IO ) {
		bar = lo & PCI_BASE_ADDRESS_IO_MASK;
	} else {
		bar = 0;
		if ( ( lo & PCI_BASE_ADDRESS_MEM_TYPE_MASK ) ==
		     PCI_BASE_ADDRESS_MEM_TYPE_64) {
			pci_read_config_dword ( dev, index + 4, &hi );
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
	return bar + pcibios_bus_base ( dev->bus );
}

/*
 * Find the size of a pci resource.
 */
unsigned long pci_bar_size ( struct pci_device *dev, unsigned int bar ) {
	uint32_t start, size;

	/* Save the original bar */
	pci_read_config_dword ( dev, bar, &start );
	/* Compute which bits can be set */
	pci_write_config_dword ( dev, bar, ~0 );
	pci_read_config_dword ( dev, bar, &size );
	/* Restore the original size */
	pci_write_config_dword ( dev, bar, start );
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
 * @dev: PCI device to query
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
int pci_find_capability ( struct pci_device *dev, int cap ) {
	uint16_t status;
	uint8_t pos, id;
	uint8_t hdr_type;
	int ttl = 48;

	pci_read_config_word ( dev, PCI_STATUS, &status );
	if ( ! ( status & PCI_STATUS_CAP_LIST ) )
		return 0;

	pci_read_config_byte ( dev, PCI_HEADER_TYPE, &hdr_type );
	switch ( hdr_type & 0x7F ) {
	case PCI_HEADER_TYPE_NORMAL:
	case PCI_HEADER_TYPE_BRIDGE:
	default:
		pci_read_config_byte ( dev, PCI_CAPABILITY_LIST, &pos );
		break;
	case PCI_HEADER_TYPE_CARDBUS:
		pci_read_config_byte ( dev, PCI_CB_CAPABILITY_LIST, &pos );
		break;
	}
	while ( ttl-- && pos >= 0x40 ) {
		pos &= ~3;
		pci_read_config_byte ( dev, pos + PCI_CAP_LIST_ID, &id );
		DBG ( "Capability: %d\n", id );
		if ( id == 0xff )
			break;
		if ( id == cap )
			return pos;
		pci_read_config_byte ( dev, pos + PCI_CAP_LIST_NEXT, &pos );
	}
	return 0;
}
