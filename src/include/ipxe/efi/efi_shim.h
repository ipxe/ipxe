#ifndef _IPXE_EFI_SHIM_H
#define _IPXE_EFI_SHIM_H

/** @file
 *
 * UEFI shim special handling
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/image.h>
#include <ipxe/efi/efi.h>

extern int efi_shim_require_loader;
extern int efi_shim_allow_pxe;
extern int efi_shim_allow_sbat;
extern struct image_tag efi_shim __image_tag;

extern int efi_shim_install ( struct image *shim, EFI_HANDLE handle,
			      wchar_t **cmdline );
extern void efi_shim_uninstall ( void );

#endif /* _IPXE_EFI_SHIM_H */
