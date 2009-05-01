#ifndef _GPXE_EFI_UMALLOC_H
#define _GPXE_EFI_UMALLOC_H

/** @file
 *
 * gPXE user memory allocation API for EFI
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#ifdef UMALLOC_EFI
#define UMALLOC_PREFIX_efi
#else
#define UMALLOC_PREFIX_efi __efi_
#endif

#endif /* _GPXE_EFI_UMALLOC_H */
