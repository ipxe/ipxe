#ifndef _MNPNET_H
#define _MNPNET_H

/** @file
 *
 * MNP NIC driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

struct efi_device;

extern int mnpnet_start ( struct efi_device *efidev );
extern void mnpnet_stop ( struct efi_device *efidev );

#endif /* _MNPNET_H */
