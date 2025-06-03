#ifndef _IPXE_EFI_ACPI_H
#define _IPXE_EFI_ACPI_H

/** @file
 *
 * iPXE ACPI API for EFI
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef ACPI_EFI
#define ACPI_PREFIX_efi
#else
#define ACPI_PREFIX_efi __efi_
#endif

/**
 * Locate ACPI table
 *
 * @v signature		Requested table signature
 * @v index		Requested index of table with this signature
 * @ret table		Table, or NULL if not found
 */
static inline __attribute__ (( always_inline )) const struct acpi_header *
ACPI_INLINE ( efi, acpi_find ) ( uint32_t signature, unsigned int index ) {

	return acpi_find_via_rsdt ( signature, index );
}

#endif /* _IPXE_EFI_ACPI_H */
