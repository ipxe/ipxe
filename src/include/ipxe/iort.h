#ifndef _IPXE_IORT_H
#define _IPXE_IORT_H

/** @file
 *
 * I/O remapping table (IORT)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>
#include <ipxe/acpi.h>
#include <ipxe/pci.h>

/** I/O remapping table signature */
#define IORT_SIGNATURE ACPI_SIGNATURE ( 'I', 'O', 'R', 'T' )

/** I/O remapping table */
struct iort_table {
	/** ACPI header */
	struct acpi_header acpi;
	/** Number of nodes */
	uint32_t count;
	/** Offset to first node */
	uint32_t offset;
	/** Reserved */
	uint8_t reserved[4];
} __attribute__ (( packed ));

/** I/O remapping table node */
struct iort_node {
	/** Type */
	uint8_t type;
	/** Length */
	uint16_t len;
	/** Revision */
	uint8_t revision;
	/** Unique identifier */
	uint32_t id;
	/** Number of ID mappings */
	uint32_t count;
	/** Offset to ID mappings */
	uint32_t offset;
} __attribute__ (( packed ));

/** I/O remapping table ID range */
struct iort_ids {
	/** Input base ID */
	uint32_t input;
	/** Number of IDs minus one */
	uint32_t limit;
	/** Output base ID */
	uint32_t output;
	/** Offset to output node */
	uint32_t offset;
	/** Flags */
	uint32_t flags;
} __attribute__ (( packed ));

/** Single output mapping */
#define IORT_FL_SINGLE 0x00000001UL

/** I/O remapping table memory access properties */
struct iort_memory {
	/** Cache coherent attribute */
	uint32_t cache;
	/** Allocation hints */
	uint8_t alloc;
	/** Reserved */
	uint8_t reserved[2];
	/** Memory access flags */
	uint8_t flags;
} __attribute__ (( packed ));

/** I/O remapping table SMMUv3 node */
#define IORT_NODE_SMMUV3 4

/** I/O remapping table SMMUv3 node */
struct iort_node_smmuv3 {
	/** Node header */
	struct iort_node node;
	/** Base address */
	uint64_t base;
	/** Flags */
	uint32_t flags;
	/** Reserved */
	uint8_t reserved[4];
	/** VATOS address */
	uint64_t vatos;
	/** Model */
	uint32_t model;
	/** Event interrupt */
	uint32_t event;
	/** PRI interrupt */
	uint32_t pri;
	/** GERR interrupt */
	uint32_t gerr;
	/** Sync interrupt */
	uint32_t sync;
	/** Proximity domain */
	uint32_t proximity;
	/** DeviceID mapping index */
	uint32_t device;
} __attribute__ (( packed ));

/** I/O remapping table PCI root complex node */
#define IORT_NODE_PCI 2

/** I/O remapping table PCI root complex node */
struct iort_node_pci {
	/** Node header */
	struct iort_node node;
	/** Memory access properties */
	struct iort_memory memory;
	/** ATS attribute */
	uint32_t ats;
	/** PCI segment number */
	uint32_t segment;
	/** Memory address size limit (in bits) */
	uint8_t memsize;
	/** PASID capabilities */
	uint16_t pasid;
	/** Reserved */
	uint8_t reserved[1];
	/** Flags */
	uint32_t flags;
} __attribute__ (( packed ));

/** An I/O remapping */
struct iort_mapping {
	/** Input node */
	const struct iort_node *input;
	/** Output node */
	const struct iort_node *output;
	/** ID mapping */
	const struct iort_ids *ids;
	/** Mapped output ID */
	uint32_t id;
};

extern int iort_map_pci ( struct pci_device *pci, struct iort_mapping *map );

#endif /* _IPXE_IORT_H */
