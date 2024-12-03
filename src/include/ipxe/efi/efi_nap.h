#ifndef _IPXE_EFI_NAP_H
#define _IPXE_EFI_NAP_H

/** @file
 *
 * CPU sleeping
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef NAP_EFI
#define NAP_PREFIX_efi
#else
#define NAP_PREFIX_efi __efi_
#endif

#endif /* _IPXE_EFI_NAP_H */
