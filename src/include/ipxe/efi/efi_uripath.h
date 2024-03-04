#ifndef _IPXE_EFI_URIPATH_H
#define _IPXE_EFI_URIPATH_H

/** @file
 *
 * EFI uri-path
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/efi/efi.h>

extern int efi_set_uri_path ( EFI_HANDLE device,
		EFI_DEVICE_PATH_PROTOCOL *path );

#endif /* _IPXE_EFI_URIPATH_H */
