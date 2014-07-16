#ifndef _IPXE_EFI_WRAP_H
#define _IPXE_EFI_WRAP_H

/** @file
 *
 * EFI driver interface
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/LoadedImage.h>

extern void efi_wrap ( EFI_HANDLE handle, EFI_LOADED_IMAGE_PROTOCOL *loaded );

#endif /* _IPXE_EFI_WRAP_H */
