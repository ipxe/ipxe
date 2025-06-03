#ifndef _IPXE_NULL_PCI_H
#define _IPXE_NULL_PCI_H

#include <stdint.h>

/** @file
 *
 * Null PCI API
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef PCIAPI_NULL
#define PCIAPI_PREFIX_null
#else
#define PCIAPI_PREFIX_null __null_
#endif

struct pci_device;

/**
 * Check if PCI bus probing is allowed
 *
 * @ret ok		Bus probing is allowed
 */
static inline __always_inline int
PCIAPI_INLINE ( null, pci_can_probe ) ( void ) {
	return 0;
}

/**
 * Find next PCI bus:dev.fn address range in system
 *
 * @v busdevfn		Starting PCI bus:dev.fn address
 * @v range		PCI bus:dev.fn address range to fill in
 */
static inline __always_inline void
PCIAPI_INLINE ( null, pci_discover ) ( uint32_t busdevfn __unused,
				       struct pci_range *range ) {

	range->start = 0;
	range->count = 0;
}

/**
 * Read byte from PCI configuration space via PCI BIOS
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value read
 * @ret rc	Return status code
 */
static inline __always_inline int
PCIAPI_INLINE ( null, pci_read_config_byte ) ( struct pci_device *pci __unused,
					       unsigned int where __unused,
					       uint8_t *value ) {
	*value = 0xff;
	return 0;
}

/**
 * Read word from PCI configuration space via PCI BIOS
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value read
 * @ret rc	Return status code
 */
static inline __always_inline int
PCIAPI_INLINE ( null, pci_read_config_word ) ( struct pci_device *pci __unused,
					       unsigned int where __unused,
					       uint16_t *value ) {
	*value = 0xffff;
	return 0;
}

/**
 * Read dword from PCI configuration space via PCI BIOS
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value read
 * @ret rc	Return status code
 */
static inline __always_inline int
PCIAPI_INLINE ( null, pci_read_config_dword ) ( struct pci_device *pci __unused,
						unsigned int where __unused,
						uint32_t *value ) {
	*value = 0xffffffff;
	return 0;
}

/**
 * Write byte to PCI configuration space via PCI BIOS
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value to be written
 * @ret rc	Return status code
 */
static inline __always_inline int
PCIAPI_INLINE ( null, pci_write_config_byte ) ( struct pci_device *pci __unused,
						unsigned int where __unused,
						uint8_t value __unused ) {
	return 0;
}

/**
 * Write word to PCI configuration space via PCI BIOS
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value to be written
 * @ret rc	Return status code
 */
static inline __always_inline int
PCIAPI_INLINE ( null, pci_write_config_word ) ( struct pci_device *pci __unused,
						unsigned int where __unused,
						uint16_t value __unused ) {
	return 0;
}

/**
 * Write dword to PCI configuration space via PCI BIOS
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value to be written
 * @ret rc	Return status code
 */
static inline __always_inline int
PCIAPI_INLINE ( null, pci_write_config_dword ) ( struct pci_device *pci
						 __unused,
						 unsigned int where __unused,
						 uint32_t value __unused ) {
	return 0;
}

/**
 * Map PCI bus address as an I/O address
 *
 * @v bus_addr		PCI bus address
 * @v len		Length of region
 * @ret io_addr		I/O address, or NULL on error
 */
static inline __always_inline void *
PCIAPI_INLINE ( null, pci_ioremap ) ( struct pci_device *pci __unused,
				      unsigned long bus_addr __unused,
				      size_t len __unused ) {
	return NULL;
}

#endif /* _IPXE_NULL_PCI_H */
