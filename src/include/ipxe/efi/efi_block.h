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

static inline __always_inline unsigned int
SANBOOT_INLINE ( efi, san_default_drive ) ( void ) {
	/* Drive numbers don't exist as a concept under EFI.  We
	 * arbitarily choose to use drive 0x80 to minimise differences
	 * with a standard BIOS.
	 */
	return 0x80;
}

#endif /* _IPXE_EFI_BLOCK_H */
