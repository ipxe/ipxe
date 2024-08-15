#ifndef _IPXE_ECAM_H
#define _IPXE_ECAM_H

/** @file
 *
 * PCI I/O API for Enhanced Configuration Access Mechanism (ECAM)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/acpi.h>
#include <ipxe/pci.h>

/** Enhanced Configuration Access Mechanism per-device size */
#define ECAM_SIZE 4096

/** Enhanced Configuration Access Mechanism table signature */
#define ECAM_SIGNATURE ACPI_SIGNATURE ( 'M', 'C', 'F', 'G' )

/** An Enhanced Configuration Access Mechanism allocation */
struct ecam_allocation {
	/** Base address */
	uint64_t base;
	/** PCI segment number */
	uint16_t segment;
	/** Start PCI bus number */
	uint8_t start;
	/** End PCI bus number */
	uint8_t end;
	/** Reserved */
	uint8_t reserved[4];
} __attribute__ (( packed ));

/** An Enhanced Configuration Access Mechanism table */
struct ecam_table {
	/** ACPI header */
	struct acpi_header acpi;
	/** Reserved */
	uint8_t reserved[8];
	/** Allocation structures */
	struct ecam_allocation alloc[0];
} __attribute__ (( packed ));

/** A mapped Enhanced Configuration Access Mechanism allocation */
struct ecam_mapping {
	/** Allocation */
	struct ecam_allocation alloc;
	/** PCI bus:dev.fn address range */
	struct pci_range range;
	/** MMIO base address */
	void *regs;
	/** Mapping result */
	int rc;
};

/**
 * Check if PCI bus probing is allowed
 *
 * @ret ok		Bus probing is allowed
 */
static inline __always_inline int
PCIAPI_INLINE ( ecam, pci_can_probe ) ( void ) {
	return 1;
}

extern struct pci_api ecam_api;

#endif /* _IPXE_ECAM_H */
