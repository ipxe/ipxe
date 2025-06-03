#ifndef _IPXE_RSDP_H
#define _IPXE_RSDP_H

/** @file
 *
 * Standard PC-BIOS ACPI RSDP interface
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef ACPI_RSDP
#define ACPI_PREFIX_rsdp
#else
#define ACPI_PREFIX_rsdp __rsdp_
#endif

/**
 * Locate ACPI table
 *
 * @v signature		Requested table signature
 * @v index		Requested index of table with this signature
 * @ret table		Table, or NULL if not found
 */
static inline __attribute__ (( always_inline )) const struct acpi_header *
ACPI_INLINE ( rsdp, acpi_find ) ( uint32_t signature, unsigned int index ) {

	return acpi_find_via_rsdt ( signature, index );
}

#endif /* _IPXE_RSDP_H */
