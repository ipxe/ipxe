#ifndef _IPXE_EFI_CACHEDHCP_H
#define _IPXE_EFI_CACHEDHCP_H

/** @file
 *
 * EFI cached DHCP packet
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/efi/efi.h>

extern int efi_cachedhcp_record ( EFI_HANDLE device );

#endif /* _IPXE_EFI_CACHEDHCP_H */
