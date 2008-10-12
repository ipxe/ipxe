#ifndef _GPXE_EFI_TIMER_H
#define _GPXE_EFI_TIMER_H

/** @file
 *
 * gPXE timer API for EFI
 *
 */

#ifdef TIMER_EFI
#define TIMER_PREFIX_efi
#else
#define TIMER_PREFIX_efi __efi_
#endif

#endif /* _GPXE_EFI_TIMER_H */
