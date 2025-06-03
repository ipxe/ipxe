#ifndef _PCIDIRECT_H
#define _PCIDIRECT_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/io.h>

#ifdef PCIAPI_DIRECT
#define PCIAPI_PREFIX_direct
#else
#define PCIAPI_PREFIX_direct __direct_
#endif

/** @file
 *
 * PCI configuration space access via Type 1 accesses
 *
 */

#define PCIDIRECT_CONFIG_ADDRESS	0xcf8
#define PCIDIRECT_CONFIG_DATA		0xcfc

struct pci_device;

extern void pcidirect_prepare ( struct pci_device *pci, int where );

/**
 * Check if PCI bus probing is allowed
 *
 * @ret ok		Bus probing is allowed
 */
static inline __always_inline int
PCIAPI_INLINE ( direct, pci_can_probe ) ( void ) {
	return 1;
}

/**
 * Find next PCI bus:dev.fn address range in system
 *
 * @v busdevfn		Starting PCI bus:dev.fn address
 * @v range		PCI bus:dev.fn address range to fill in
 */
static inline __always_inline void
PCIAPI_INLINE ( direct, pci_discover ) ( uint32_t busdevfn __unused,
					 struct pci_range *range ) {

	/* Scan first bus and rely on bridge detection to find higher buses */
	range->start = PCI_BUSDEVFN ( 0, 0, 0, 0 );
	range->count = PCI_BUSDEVFN ( 0, 1, 0, 0 );
}

/**
 * Read byte from PCI configuration space via Type 1 access
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value read
 * @ret rc	Return status code
 */
static inline __always_inline int
PCIAPI_INLINE ( direct, pci_read_config_byte ) ( struct pci_device *pci,
						 unsigned int where,
						 uint8_t *value ) {
	pcidirect_prepare ( pci, where );
	*value = inb ( PCIDIRECT_CONFIG_DATA + ( where & 3 ) );
	return 0;
}

/**
 * Read word from PCI configuration space via Type 1 access
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value read
 * @ret rc	Return status code
 */
static inline __always_inline int
PCIAPI_INLINE ( direct, pci_read_config_word ) ( struct pci_device *pci,
						 unsigned int where,
						 uint16_t *value ) {
	pcidirect_prepare ( pci, where );
	*value = inw ( PCIDIRECT_CONFIG_DATA + ( where & 2 ) );
	return 0;
}

/**
 * Read dword from PCI configuration space via Type 1 access
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value read
 * @ret rc	Return status code
 */
static inline __always_inline int
PCIAPI_INLINE ( direct, pci_read_config_dword ) ( struct pci_device *pci,
						  unsigned int where,
						  uint32_t *value ) {
	pcidirect_prepare ( pci, where );
	*value = inl ( PCIDIRECT_CONFIG_DATA );
	return 0;
}

/**
 * Write byte to PCI configuration space via Type 1 access
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value to be written
 * @ret rc	Return status code
 */
static inline __always_inline int
PCIAPI_INLINE ( direct, pci_write_config_byte ) ( struct pci_device *pci,
						  unsigned int where,
						  uint8_t value ) {
	pcidirect_prepare ( pci, where );
	outb ( value, PCIDIRECT_CONFIG_DATA + ( where & 3 ) );
	return 0;
}

/**
 * Write word to PCI configuration space via Type 1 access
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value to be written
 * @ret rc	Return status code
 */
static inline __always_inline int
PCIAPI_INLINE ( direct, pci_write_config_word ) ( struct pci_device *pci,
						  unsigned int where,
						  uint16_t value ) {
	pcidirect_prepare ( pci, where );
	outw ( value, PCIDIRECT_CONFIG_DATA + ( where & 2 ) );
	return 0;
}

/**
 * Write dword to PCI configuration space via Type 1 access
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value to be written
 * @ret rc	Return status code
 */
static inline __always_inline int
PCIAPI_INLINE ( direct, pci_write_config_dword ) ( struct pci_device *pci,
						   unsigned int where,
						   uint32_t value ) {
	pcidirect_prepare ( pci, where );
	outl ( value, PCIDIRECT_CONFIG_DATA );
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
PCIAPI_INLINE ( direct, pci_ioremap ) ( struct pci_device *pci __unused,
					unsigned long bus_addr, size_t len ) {
	return ioremap ( bus_addr, len );
}

extern struct pci_api pcidirect_api;

#endif /* _PCIDIRECT_H */
