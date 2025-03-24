#ifndef _IPXE_EFI_PCI_H
#define _IPXE_EFI_PCI_H

/** @file
 *
 * EFI driver interface
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/pci.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/PciIo.h>

/* PciRootBridgeIo.h uses LShiftU64(), which isn't defined anywhere else */
static inline EFIAPI uint64_t LShiftU64 ( UINT64 value, UINTN shift ) {
	return ( value << shift );
}

/** An EFI PCI device */
struct efi_pci_device {
	/** PCI device */
	struct pci_device pci;
	/** PCI I/O protocol */
	EFI_PCI_IO_PROTOCOL *io;
};

extern int efipci_info ( EFI_HANDLE device, struct efi_pci_device *efipci );

#endif /* _IPXE_EFI_PCI_H */
