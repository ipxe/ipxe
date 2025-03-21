#ifndef _IPXE_EFI_WRAP_H
#define _IPXE_EFI_WRAP_H

/** @file
 *
 * EFI driver interface
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/efi/efi.h>

extern void efi_wrap_bs ( EFI_BOOT_SERVICES *wrapped );
extern void efi_wrap_systab ( int global );
extern void efi_unwrap ( void );

extern void efi_wrap_image ( EFI_HANDLE handle );

#endif /* _IPXE_EFI_WRAP_H */
