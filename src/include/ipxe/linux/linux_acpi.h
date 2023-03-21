#ifndef _IPXE_LINUX_ACPI_H
#define _IPXE_LINUX_ACPI_H

/** @file
 *
 * iPXE ACPI API for Linux
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef ACPI_LINUX
#define ACPI_PREFIX_linux
#else
#define ACPI_PREFIX_linux __linux_
#endif

#endif /* _IPXE_LINUX_ACPI_H */
