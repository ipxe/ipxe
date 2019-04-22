#ifndef _IPXE_PCIMSIX_H
#define _IPXE_PCIMSIX_H

/** @file
 *
 * PCI MSI-X interrupts
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/pci.h>

/** MSI-X BAR mapped length */
#define PCI_MSIX_LEN 0x1000

/** MSI-X vector offset */
#define PCI_MSIX_VECTOR(n) ( (n) * 0x10 )

/** MSI-X vector address low 32 bits */
#define PCI_MSIX_ADDRESS_LO 0x0

/** MSI-X vector address high 32 bits */
#define PCI_MSIX_ADDRESS_HI 0x4

/** MSI-X vector data */
#define PCI_MSIX_DATA 0x8

/** MSI-X vector control */
#define PCI_MSIX_CONTROL 0xc
#define PCI_MSIX_CONTROL_MASK 0x00000001	/**< Vector is masked */

/** PCI MSI-X capability */
struct pci_msix {
	/** Capability offset */
	unsigned int cap;
	/** Number of vectors */
	unsigned int count;
	/** MSI-X table */
	void *table;
	/** Pending bit array */
	void *pba;
};

extern int pci_msix_enable ( struct pci_device *pci, struct pci_msix *msix );
extern void pci_msix_disable ( struct pci_device *pci, struct pci_msix *msix );
extern void pci_msix_map ( struct pci_msix *msix, unsigned int vector,
			   physaddr_t address, uint32_t data );
extern void pci_msix_control ( struct pci_msix *msix, unsigned int vector,
			       uint32_t mask );
extern void pci_msix_dump ( struct pci_msix *msix, unsigned int vector );

/**
 * Mask MSI-X interrupt vector
 *
 * @v msix		MSI-X capability
 * @v vector		MSI-X vector
 */
static inline __attribute__ (( always_inline )) void
pci_msix_mask ( struct pci_msix *msix, unsigned int vector ) {

	pci_msix_control ( msix, vector, PCI_MSIX_CONTROL_MASK );
}

/**
 * Unmask MSI-X interrupt vector
 *
 * @v msix		MSI-X capability
 * @v vector		MSI-X vector
 */
static inline __attribute__ (( always_inline )) void
pci_msix_unmask ( struct pci_msix *msix, unsigned int vector ) {

	pci_msix_control ( msix, vector, 0 );
}

#endif /* _IPXE_PCIMSIX_H */
