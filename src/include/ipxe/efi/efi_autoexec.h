#ifndef _IPXE_EFI_AUTOEXEC_H
#define _IPXE_EFI_AUTOEXEC_H

/** @file
 *
 * EFI autoexec script
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/efi/efi.h>

extern int efi_autoexec_load ( EFI_HANDLE device );

#endif /* _IPXE_EFI_AUTOEXEC_H */
