#ifndef _IPXE_PCI_IO_H
#define _IPXE_PCI_IO_H

/** @file
 *
 * PCI I/O API
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>
#include <ipxe/api.h>
#include <ipxe/tables.h>
#include <ipxe/iomap.h>
#include <config/ioapi.h>

struct pci_device;
struct pci_api;

/** A PCI bus:dev.fn address range */
struct pci_range {
	/** Starting bus:dev.fn address */
	uint32_t start;
	/** Number of bus:dev.fn addresses within this range */
	unsigned int count;
};

#define PCI_BUSDEVFN( segment, bus, slot, func )			\
	( ( (segment) << 16 ) | ( (bus) << 8 ) |			\
	  ( (slot) << 3 ) | ( (func) << 0 ) )

/**
 * Calculate static inline PCI I/O API function name
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 * @ret _subsys_func	Subsystem API function
 */
#define PCIAPI_INLINE( _subsys, _api_func ) \
	SINGLE_API_INLINE ( PCIAPI_PREFIX_ ## _subsys, _api_func )

/**
 * Provide a PCI I/O API implementation
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 * @v _func		Implementing function
 */
#define PROVIDE_PCIAPI( _subsys, _api_func, _func ) \
	PROVIDE_SINGLE_API ( PCIAPI_PREFIX_ ## _subsys, _api_func, _func )

/**
 * Provide a static inline PCI I/O API implementation
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 */
#define PROVIDE_PCIAPI_INLINE( _subsys, _api_func ) \
	PROVIDE_SINGLE_API_INLINE ( PCIAPI_PREFIX_ ## _subsys, _api_func )

/* Include all architecture-independent I/O API headers */
#include <ipxe/null_pci.h>
#include <ipxe/ecam_io.h>
#include <ipxe/pcicloud.h>
#include <ipxe/efi/efi_pci_api.h>
#include <ipxe/linux/linux_pci.h>

/* Include all architecture-dependent I/O API headers */
#include <bits/pci_io.h>

/**
 * Check if PCI bus probing is allowed
 *
 * @v pci		PCI device
 * @ret ok		Bus probing is allowed
 */
int pci_can_probe ( struct pci_device *pci );

/**
 * Find next PCI bus:dev.fn address range in system
 *
 * @v busdevfn		Starting PCI bus:dev.fn address
 * @v range		PCI bus:dev.fn address range to fill in
 */
void pci_discover ( uint32_t busdevfn, struct pci_range *range );

/**
 * Read byte from PCI configuration space
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value read
 * @ret rc	Return status code
 */
int pci_read_config_byte ( struct pci_device *pci, unsigned int where,
			   uint8_t *value );

/**
 * Read 16-bit word from PCI configuration space
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value read
 * @ret rc	Return status code
 */
int pci_read_config_word ( struct pci_device *pci, unsigned int where,
			   uint16_t *value );

/**
 * Read 32-bit dword from PCI configuration space
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value read
 * @ret rc	Return status code
 */
int pci_read_config_dword ( struct pci_device *pci, unsigned int where,
			    uint32_t *value );

/**
 * Write byte to PCI configuration space
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value to be written
 * @ret rc	Return status code
 */
int pci_write_config_byte ( struct pci_device *pci, unsigned int where,
			    uint8_t value );

/**
 * Write 16-bit word to PCI configuration space
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value to be written
 * @ret rc	Return status code
 */
int pci_write_config_word ( struct pci_device *pci, unsigned int where,
			    uint16_t value );

/**
 * Write 32-bit dword to PCI configuration space
 *
 * @v pci	PCI device
 * @v where	Location within PCI configuration space
 * @v value	Value to be written
 * @ret rc	Return status code
 */
int pci_write_config_dword ( struct pci_device *pci, unsigned int where,
			     uint32_t value );

/**
 * Map PCI bus address as an I/O address
 *
 * @v bus_addr		PCI bus address
 * @v len		Length of region
 * @ret io_addr		I/O address, or NULL on error
 */
void * pci_ioremap ( struct pci_device *pci, unsigned long bus_addr,
		     size_t len );

/** A runtime selectable PCI I/O API */
struct pci_api {
	const char *name;
	typeof ( pci_can_probe ) ( * pci_can_probe );
	typeof ( pci_discover ) ( * pci_discover );
	typeof ( pci_read_config_byte ) ( * pci_read_config_byte );
	typeof ( pci_read_config_word ) ( * pci_read_config_word );
	typeof ( pci_read_config_dword ) ( * pci_read_config_dword );
	typeof ( pci_write_config_byte ) ( * pci_write_config_byte );
	typeof ( pci_write_config_word ) ( * pci_write_config_word );
	typeof ( pci_write_config_dword ) ( * pci_write_config_dword );
	typeof ( pci_ioremap ) ( * pci_ioremap );
};

/** Runtime selectable PCI API table */
#define PCI_APIS __table ( struct pci_api, "pci_apis" )

/**
 * Declare a runtime selectable PCI API
 *
 * In the common case of a non-runtime-selectable PCI I/O API, allow
 * the runtime API code to be garbage-collected at link time to save
 * space.
 */
#ifdef PCIAPI_CLOUD
#define __pci_api( priority ) __table_entry ( PCI_APIS, priority )
#else
#define __pci_api( priority )
#endif

/* PCI runtime selectable API priorities */
#define PCIAPI_PRIORITY_EFI	01	/**< EFI PCI I/O protocols */
#define PCIAPI_PRIORITY_ECAM	02	/**< ACPI ECAM */
#define PCIAPI_PRIORITY_PCBIOS	03	/**< PCI BIOS calls */
#define PCIAPI_PRIORITY_DIRECT	04	/**< Direct Type 1 accesses */

/** Provide a runtime selectable PCI I/O API */
#define PROVIDE_PCIAPI_RUNTIME( subsys, priority )			   \
	struct pci_api pciapi_ ## subsys __pci_api ( priority ) = {	   \
		.name = #subsys,					   \
		.pci_can_probe = PCIAPI_INLINE ( subsys, pci_can_probe ),  \
		.pci_discover = PCIAPI_INLINE ( subsys, pci_discover ),	   \
		.pci_read_config_byte =					   \
			PCIAPI_INLINE ( subsys, pci_read_config_byte ),	   \
		.pci_read_config_word =					   \
			PCIAPI_INLINE ( subsys, pci_read_config_word ),	   \
		.pci_read_config_dword =				   \
			PCIAPI_INLINE ( subsys, pci_read_config_dword ),   \
		.pci_write_config_byte =				   \
			PCIAPI_INLINE ( subsys, pci_write_config_byte ),   \
		.pci_write_config_word =				   \
			PCIAPI_INLINE ( subsys, pci_write_config_word ),   \
		.pci_write_config_dword =				   \
			PCIAPI_INLINE ( subsys, pci_write_config_dword ),  \
		.pci_ioremap = PCIAPI_INLINE ( subsys, pci_ioremap ),	   \
	}

#endif /* _IPXE_PCI_IO_H */
