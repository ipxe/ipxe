#ifndef _IPXE_ECAM_IO_H
#define _IPXE_ECAM_IO_H

/** @file
 *
 * PCI I/O API for Enhanced Configuration Access Mechanism (ECAM)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

#ifdef PCIAPI_ECAM
#define PCIAPI_PREFIX_ecam
#else
#define PCIAPI_PREFIX_ecam __ecam_
#endif

struct pci_device;

/** Construct ECAM location */
#define ECAM_LOC( where, len ) ( ( (len) << 16 ) | where )

/** Extract offset from ECAM location */
#define ECAM_WHERE( location ) ( (location) & 0xffff )

/** Extract length from ECAM location */
#define ECAM_LEN( location ) ( (location) >> 16 )

extern int ecam_read ( struct pci_device *pci, unsigned int location,
		       void *value );
extern int ecam_write ( struct pci_device *pci, unsigned int location,
			unsigned long value );

/**
 * Read byte from PCI configuration space via ECAM
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value read
 * @ret rc	Return status code
 */
static inline __always_inline int
PCIAPI_INLINE ( ecam, pci_read_config_byte ) ( struct pci_device *pci,
					       unsigned int where,
					       uint8_t *value ) {
	return ecam_read ( pci, ECAM_LOC ( where, sizeof ( *value ) ), value );
}

/**
 * Read word from PCI configuration space via ECAM
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value read
 * @ret rc	Return status code
 */
static inline __always_inline int
PCIAPI_INLINE ( ecam, pci_read_config_word ) ( struct pci_device *pci,
					       unsigned int where,
					       uint16_t *value ) {
	return ecam_read ( pci, ECAM_LOC ( where, sizeof ( *value ) ), value );
}

/**
 * Read dword from PCI configuration space via ECAM
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value read
 * @ret rc	Return status code
 */
static inline __always_inline int
PCIAPI_INLINE ( ecam, pci_read_config_dword ) ( struct pci_device *pci,
						unsigned int where,
						uint32_t *value ) {
	return ecam_read ( pci, ECAM_LOC ( where, sizeof ( *value ) ), value );
}

/**
 * Write byte to PCI configuration space via ECAM
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value to be written
 * @ret rc	Return status code
 */
static inline __always_inline int
PCIAPI_INLINE ( ecam, pci_write_config_byte ) ( struct pci_device *pci,
						unsigned int where,
						uint8_t value ) {
	return ecam_write ( pci, ECAM_LOC ( where, sizeof ( value ) ), value );
}

/**
 * Write word to PCI configuration space via ECAM
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value to be written
 * @ret rc	Return status code
 */
static inline __always_inline int
PCIAPI_INLINE ( ecam, pci_write_config_word ) ( struct pci_device *pci,
						unsigned int where,
						uint16_t value ) {
	return ecam_write ( pci, ECAM_LOC ( where, sizeof ( value ) ), value );
}

/**
 * Write dword to PCI configuration space via ECAM
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value to be written
 * @ret rc	Return status code
 */
static inline __always_inline int
PCIAPI_INLINE ( ecam, pci_write_config_dword ) ( struct pci_device *pci,
						 unsigned int where,
						 uint32_t value ) {
	return ecam_write ( pci, ECAM_LOC ( where, sizeof ( value ) ), value );
}

/**
 * Map PCI bus address as an I/O address
 *
 * @v bus_addr		PCI bus address
 * @v len		Length of region
 * @ret io_addr		I/O address, or NULL on error
 */
static inline __always_inline void *
PCIAPI_INLINE ( ecam, pci_ioremap ) ( struct pci_device *pci __unused,
				      unsigned long bus_addr, size_t len ) {
	return ioremap ( bus_addr, len );
}

#endif /* _IPXE_ECAM_IO_H */
