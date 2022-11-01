#ifndef _IPXE_EFI_WRAP_H
#define _IPXE_EFI_WRAP_H

/** @file
 *
 * EFI driver interface
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/efi/efi.h>

extern EFI_SYSTEM_TABLE * efi_wrap_systab ( void );
extern void efi_wrap ( EFI_HANDLE handle );

#endif /* _IPXE_EFI_WRAP_H */
