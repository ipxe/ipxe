#ifndef _IPXE_UPL_ACPI_H
#define _IPXE_UPL_ACPI_H

/** @file
 *
 * Universal Payload ACPI interface
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#ifdef ACPI_UPL
#define ACPI_PREFIX_upl
#else
#define ACPI_PREFIX_upl __upl_
#endif

/**
 * Locate ACPI table
 *
 * @v signature		Requested table signature
 * @v index		Requested index of table with this signature
 * @ret table		Table, or NULL if not found
 */
static inline __attribute__ (( always_inline )) const struct acpi_header *
ACPI_INLINE ( upl, acpi_find ) ( uint32_t signature, unsigned int index ) {

	return acpi_find_via_rsdt ( signature, index );
}

#endif /* _IPXE_UPL_ACPI_H */
