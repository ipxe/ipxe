#ifndef _IPXE_EFI_SNP_H
#define _IPXE_EFI_SNP_H

/** @file
 *
 * iPXE EFI SNP interface
 *
 */

#include <ipxe/list.h>
#include <ipxe/netdevice.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/SimpleNetwork.h>
#include <ipxe/efi/Protocol/NetworkInterfaceIdentifier.h>
#include <ipxe/efi/Protocol/ComponentName2.h>
#include <ipxe/efi/Protocol/DevicePath.h>
#include <ipxe/efi/Protocol/HiiConfigAccess.h>
#include <ipxe/efi/Protocol/HiiDatabase.h>

/** An SNP device */
struct efi_snp_device {
	/** List of SNP devices */
	struct list_head list;
	/** The underlying iPXE network device */
	struct net_device *netdev;
	/** The underlying EFI PCI device */
	struct efi_pci_device *efipci;
	/** EFI device handle */
	EFI_HANDLE handle;
	/** The SNP structure itself */
	EFI_SIMPLE_NETWORK_PROTOCOL snp;
	/** The SNP "mode" (parameters) */
	EFI_SIMPLE_NETWORK_MODE mode;
	/** Outstanding TX packet count (via "interrupt status")
	 *
	 * Used in order to generate TX completions.
	 */
	unsigned int tx_count_interrupts;
	/** Outstanding TX packet count (via "recycled tx buffers")
	 *
	 * Used in order to generate TX completions.
	 */
	unsigned int tx_count_txbufs;
	/** Outstanding RX packet count (via "interrupt status") */
	unsigned int rx_count_interrupts;
	/** Outstanding RX packet count (via WaitForPacket event) */
	unsigned int rx_count_events;
	/** The network interface identifier */
	EFI_NETWORK_INTERFACE_IDENTIFIER_PROTOCOL nii;
	/** Component name protocol */
	EFI_COMPONENT_NAME2_PROTOCOL name2;
	/** HII configuration access protocol */
	EFI_HII_CONFIG_ACCESS_PROTOCOL hii;
	/** HII package list */
	EFI_HII_PACKAGE_LIST_HEADER *package_list;
	/** HII handle */
	EFI_HII_HANDLE hii_handle;
	/** Device name */
	wchar_t name[ sizeof ( ( ( struct net_device * ) NULL )->name ) ];
	/** Driver name */
	wchar_t driver_name[16];
	/** Controller name */
	wchar_t controller_name[32];
	/** The device path
	 *
	 * This field is variable in size and must appear at the end
	 * of the structure.
	 */
	EFI_DEVICE_PATH_PROTOCOL path;
};

extern int efi_snp_hii_install ( struct efi_snp_device *snpdev );
extern void efi_snp_hii_uninstall ( struct efi_snp_device *snpdev );

#endif /* _IPXE_EFI_SNP_H */
