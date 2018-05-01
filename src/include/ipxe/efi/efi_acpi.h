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

#endif /* _IPXE_EFI_ACPI_H */
