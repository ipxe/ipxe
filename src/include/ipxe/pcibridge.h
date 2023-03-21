#ifndef _IPXE_PCIBRIDGE_H
#define _IPXE_PCIBRIDGE_H

/** @file
 *
 * PCI-to-PCI bridge
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/list.h>
#include <ipxe/pci.h>

/** A PCI-to-PCI bridge */
struct pci_bridge {
	/** PCI device */
	struct pci_device *pci;
	/** Bridge numbers */
	union {
		/** Raw dword */
		uint32_t buses;
		struct {
			/** Primary bus */
			uint8_t primary;
			/** Secondary bus */
			uint8_t secondary;
			/** Subordinate bus */
			uint8_t subordinate;
		} __attribute__ (( packed ));
	};
	/** Memory base */
	uint32_t membase;
	/** Memory limit */
	uint32_t memlimit;
	/** List of bridges */
	struct list_head list;
};

extern struct pci_bridge * pcibridge_find ( struct pci_device *pci );

#endif /* _IPXE_PCIBRIDGE_H */
