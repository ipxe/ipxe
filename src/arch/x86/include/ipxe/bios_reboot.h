#ifndef _IPXE_BIOS_REBOOT_H
#define _IPXE_BIOS_REBOOT_H

/** @file
 *
 * Standard PC-BIOS reboot mechanism
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#ifdef REBOOT_PCBIOS
#define REBOOT_PREFIX_pcbios
#else
#define REBOOT_PREFIX_pcbios __pcbios_
#endif

#endif /* _IPXE_BIOS_REBOOT_H */
