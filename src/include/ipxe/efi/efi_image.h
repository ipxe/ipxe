#ifndef _IPXE_EFI_IMAGE_H
#define _IPXE_EFI_IMAGE_H

/** @file
 *
 * EFI images
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/image.h>

extern struct image_type efi_image_type[] __image_type ( PROBE_NORMAL );

/**
 * Check if EFI image can be loaded directly
 *
 * @v image		EFI image
 * @ret can_load	EFI image can be loaded directly
 */
static inline int efi_can_load ( struct image *image ) {

	return ( image->type == efi_image_type );
}

#endif /* _IPXE_EFI_IMAGE_H */
