#ifndef _IPXE_EFI_NULL_H
#define _IPXE_EFI_NULL_H

/** @file
 *
 * EFI null interfaces
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/AppleNetBoot.h>
#include <ipxe/efi/Protocol/BlockIo.h>
#include <ipxe/efi/Protocol/ComponentName2.h>
#include <ipxe/efi/Protocol/HiiConfigAccess.h>
#include <ipxe/efi/Protocol/LoadFile.h>
#include <ipxe/efi/Protocol/NetworkInterfaceIdentifier.h>
#include <ipxe/efi/Protocol/PxeBaseCode.h>
#include <ipxe/efi/Protocol/SimpleNetwork.h>
#include <ipxe/efi/Protocol/UsbIo.h>

extern void efi_nullify_snp ( EFI_SIMPLE_NETWORK_PROTOCOL *snp );
extern void efi_nullify_nii ( EFI_NETWORK_INTERFACE_IDENTIFIER_PROTOCOL *nii );
extern void efi_nullify_name2 ( EFI_COMPONENT_NAME2_PROTOCOL *name2 );
extern void efi_nullify_load_file ( EFI_LOAD_FILE_PROTOCOL *load_file );
extern void efi_nullify_hii ( EFI_HII_CONFIG_ACCESS_PROTOCOL *hii );
extern void efi_nullify_block ( EFI_BLOCK_IO_PROTOCOL *block );
extern void efi_nullify_pxe ( EFI_PXE_BASE_CODE_PROTOCOL *pxe );
extern void efi_nullify_apple ( EFI_APPLE_NET_BOOT_PROTOCOL *apple );
extern void efi_nullify_usbio ( EFI_USB_IO_PROTOCOL *usbio );

#endif /* _IPXE_EFI_NULL_H */
