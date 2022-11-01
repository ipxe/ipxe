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

#endif /* _IPXE_RSDP_H */
