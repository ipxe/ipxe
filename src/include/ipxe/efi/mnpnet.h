#ifndef _IPXE_EFI_MNPNET_H
#define _IPXE_EFI_MNPNET_H

/** @file
 *
 * MNP NIC driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

struct efi_device;
struct net_device;

extern int mnpnet_start ( struct efi_device *efidev );
extern void mnpnet_stop ( struct efi_device *efidev );
extern int mnptemp_create ( EFI_HANDLE handle, struct net_device **netdev );
extern void mnptemp_destroy ( struct net_device *netdev );

#endif /* _IPXE_EFI_MNPNET_H */
