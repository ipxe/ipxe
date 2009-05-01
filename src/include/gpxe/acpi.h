#ifndef _GPXE_ACPI_H
#define _GPXE_ACPI_H

/** @file
 *
 * ACPI data structures
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>

/**
 * An ACPI description header
 *
 * This is the structure common to the start of all ACPI system
 * description tables.
 */
struct acpi_description_header {
	/** ACPI signature (4 ASCII characters) */
	char signature[4];
	/** Length of table, in bytes, including header */
	uint32_t length;
	/** ACPI Specification minor version number */
	uint8_t revision;
	/** To make sum of entire table == 0 */
	uint8_t checksum;
	/** OEM identification */
	char oem_id[6];
	/** OEM table identification */
	char oem_table_id[8];
	/** OEM revision number */
	uint32_t oem_revision;
	/** ASL compiler vendor ID */
	char asl_compiler_id[4];
	/** ASL compiler revision number */
	uint32_t asl_compiler_revision;
} __attribute__ (( packed ));

extern void acpi_fix_checksum ( struct acpi_description_header *acpi );

#endif /* _GPXE_ACPI_H */
