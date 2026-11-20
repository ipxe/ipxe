#ifndef _IPXE_UPL_H
#define _IPXE_UPL_H

/** @file
 *
 * Universal Payload (UPL) interface
 *
 * The bootloader passes a Flattened Device Tree describing the memory
 * map, serial port, ACPI tables, SMBIOS tables, and graphics frame
 * buffer.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

extern uint64_t upl_mapped;

extern int upl_fdt_find ( const char *prefix, const char *compatible,
			  unsigned int *offset );
extern int upl_fdt_reserved ( const char *compatible, unsigned int index,
			      uint64_t *start );
extern physaddr_t upl_acpi_rsdp ( void );

#endif /* _IPXE_UPL_H */
