#ifndef _IPXE_NULL_ACPI_H
#define _IPXE_NULL_ACPI_H

/** @file
 *
 * Standard do-nothing ACPI interface
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stddef.h>

#ifdef ACPI_NULL
#define ACPI_PREFIX_null
#else
#define ACPI_PREFIX_null __null_
#endif

static inline __attribute__ (( always_inline )) const struct acpi_header *
ACPI_INLINE ( null, acpi_find ) ( uint32_t signature __unused,
				  unsigned int index __unused ) {

	return NULL;
}

#endif /* _IPXE_NULL_ACPI_H */
