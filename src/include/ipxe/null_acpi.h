#ifndef _IPXE_NULL_ACPI_H
#define _IPXE_NULL_ACPI_H

/** @file
 *
 * Standard do-nothing ACPI interface
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef ACPI_NULL
#define ACPI_PREFIX_null
#else
#define ACPI_PREFIX_null __null_
#endif

static inline __always_inline userptr_t
ACPI_INLINE ( null, acpi_find_rsdt ) ( void ) {
	return UNULL;
}

#endif /* _IPXE_NULL_ACPI_H */
