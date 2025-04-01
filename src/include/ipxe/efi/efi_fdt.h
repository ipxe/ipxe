#ifndef _IPXE_EFI_FDT_H
#define _IPXE_EFI_FDT_H

/** @file
 *
 * EFI Flattened Device Tree
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/efi/efi.h>

extern int efi_fdt_install ( const char *cmdline );
extern int efi_fdt_uninstall ( void );

#endif /* _IPXE_EFI_FDT_H */
