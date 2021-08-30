#ifndef _IPXE_EFI_AUTOBOOT_H
#define _IPXE_EFI_AUTOBOOT_H

/** @file
 *
 * EFI autoboot device
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/efi/efi.h>

extern int efi_set_autoboot_ll_addr ( EFI_HANDLE device );

#endif /* _IPXE_EFI_AUTOBOOT_H */
