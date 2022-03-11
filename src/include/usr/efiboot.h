#ifndef _USR_EFIBOOT_H
#define _USR_EFIBOOT_H

/** @file
 *
 * EFI boot support
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

extern void efi_boot_display_map ( void );
extern int efi_boot_local ( unsigned int drive, const char *filename );

#endif /* _USR_EFIBOOT_H */
