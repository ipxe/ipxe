#ifndef _IPXE_EFI_FILE_H
#define _IPXE_EFI_FILE_H

/** @file
 *
 * EFI file protocols
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

extern int efi_file_install ( EFI_HANDLE handle );
extern void efi_file_uninstall ( EFI_HANDLE handle );

#endif /* _IPXE_EFI_FILE_H */
