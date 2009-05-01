#ifndef _GPXE_EFI_SMBIOS_H
#define _GPXE_EFI_SMBIOS_H

/** @file
 *
 * gPXE SMBIOS API for EFI
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#ifdef SMBIOS_EFI
#define SMBIOS_PREFIX_efi
#else
#define SMBIOS_PREFIX_efi __efi_
#endif

#endif /* _GPXE_EFI_SMBIOS_H */
