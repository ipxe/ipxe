#ifndef _IPXE_EFI_BLOCK_H
#define _IPXE_EFI_BLOCK_H

/** @block
 *
 * EFI block device protocols
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef SANBOOT_EFI
#define SANBOOT_PREFIX_efi
#else
#define SANBOOT_PREFIX_efi __efi_
#endif

#endif /* _IPXE_EFI_BLOCK_H */
