#ifndef _IPXE_EFI_SERVICE_H
#define _IPXE_EFI_SERVICE_H

/** @file
 *
 * EFI service binding
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/efi/efi.h>

extern int efi_service_add ( EFI_HANDLE service, EFI_GUID *binding,
			     EFI_HANDLE *handle );
extern int efi_service_del ( EFI_HANDLE service, EFI_GUID *binding,
			     EFI_HANDLE handle );

#endif /* _IPXE_EFI_SERVICE_H */
