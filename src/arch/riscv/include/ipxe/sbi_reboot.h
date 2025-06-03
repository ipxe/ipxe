#ifndef _IPXE_BIOS_REBOOT_H
#define _IPXE_BIOS_REBOOT_H

/** @file
 *
 * Supervisor Binary Interface (SBI) reboot mechanism
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef REBOOT_SBI
#define REBOOT_PREFIX_sbi
#else
#define REBOOT_PREFIX_sbi __sbi_
#endif

#endif /* _IPXE_BIOS_REBOOT_H */
