#ifndef _IPXE_EFI_PATH_H
#define _IPXE_EFI_PATH_H

/** @file
 *
 * EFI device paths
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/interface.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/DevicePath.h>

struct net_device;
struct uri;
struct aoe_device;
struct usb_function;

extern EFI_DEVICE_PATH_PROTOCOL *
efi_path_end ( EFI_DEVICE_PATH_PROTOCOL *path );
extern size_t efi_path_len ( EFI_DEVICE_PATH_PROTOCOL *path );
extern EFI_DEVICE_PATH_PROTOCOL * efi_paths ( EFI_DEVICE_PATH_PROTOCOL *first,
					      ... );
extern EFI_DEVICE_PATH_PROTOCOL * efi_netdev_path ( struct net_device *netdev );
extern EFI_DEVICE_PATH_PROTOCOL * efi_uri_path ( struct uri *uri );
extern EFI_DEVICE_PATH_PROTOCOL * efi_aoe_path ( struct aoe_device *aoedev );
extern EFI_DEVICE_PATH_PROTOCOL * efi_usb_path ( struct usb_function *func );

extern EFI_DEVICE_PATH_PROTOCOL * efi_describe ( struct interface *interface );
#define efi_describe_TYPE( object_type ) \
	typeof ( EFI_DEVICE_PATH_PROTOCOL * ( object_type ) )

#endif /* _IPXE_EFI_PATH_H */
