#ifndef _GPXE_BIOS_SMBIOS_H
#define _GPXE_BIOS_SMBIOS_H

/** @file
 *
 * Standard PC-BIOS SMBIOS interface
 *
 */

#ifdef SMBIOS_PCBIOS
#define SMBIOS_PREFIX_pcbios
#else
#define SMBIOS_PREFIX_pcbios __pcbios_
#endif

#endif /* _GPXE_BIOS_SMBIOS_H */
