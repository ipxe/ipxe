#ifndef _IPXE_SPCR_H
#define _IPXE_SPCR_H

/** @file
 *
 * ACPI Serial Port Console Redirection (SPCR)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/acpi.h>

/** Serial Port Console Redirection table signature */
#define SPCR_SIGNATURE ACPI_SIGNATURE ( 'S', 'P', 'C', 'R' )

/** A Serial Port Console Redirection table */
struct spcr_table {
	/** ACPI header */
	struct acpi_header acpi;
	/** Interface type */
	uint8_t type;
	/** Reserved */
	uint8_t reserved_a[3];
	/** Base address */
	struct acpi_address base;
	/** Reserved */
	uint8_t reserved_b[6];
	/** Baud rate
	 *
	 *  0: leave unchanged
	 *  1:   2400 = 115200 / 48   (not defined in standard)
	 *  2:   4800 = 115200 / 24   (not defined in standard)
	 *  3:   9600 = 115200 / 12
	 *  4:  19200 = 115200 / 6
	 *  5:  38400 = 115200 / 3    (not defined in standard)
	 *  6:  57600 = 115200 / 2
	 *  7: 115200 = 115200 / 1
	 */
	uint8_t baud;
	/** Parity */
	uint8_t parity;
	/** Stop bits */
	uint8_t stop;
	/** Flow control */
	uint8_t flow;
	/** Terminal type */
	uint8_t terminal;
	/** Language */
	uint8_t lang;
	/** PCI device ID */
	uint16_t pci_device_id;
	/** PCI vendor ID */
	uint16_t pci_vendor_id;
	/** PCI bus number */
	uint8_t pci_bus;
	/** PCI device number */
	uint8_t pci_dev;
	/** PCI function number */
	uint8_t pci_func;
	/** Reserved */
	uint8_t reserved_c[4];
	/** PCI segment */
	uint8_t pci_segment;
	/** Clock frequency */
	uint32_t clock;
	/** Precise baud rate */
	uint32_t precise;
	/** Reserved */
	uint8_t reserved_d[4];
} __attribute__ (( packed ));

/* SPCR interface types */
#define SPCR_TYPE_16550		0x0000		/**< 16550-compatible */
#define SPCR_TYPE_16450		0x0001		/**< 16450-compatible */
#define SPCR_TYPE_16550_GAS	0x0012		/**< 16550-compatible */

/** SPCR baud rates */
enum spcr_baud {
	SPCR_BAUD_2400 = 1,
	SPCR_BAUD_4800 = 2,
	SPCR_BAUD_9600 = 3,
	SPCR_BAUD_19200 = 4,
	SPCR_BAUD_38400 = 5,
	SPCR_BAUD_57600 = 6,
	SPCR_BAUD_115200 = 7,
	SPCR_BAUD_MAX
};

#endif /* _IPXE_SPCR_H */
