#ifndef CONFIG_DEFAULTS_EFI_H
#define CONFIG_DEFAULTS_EFI_H

/** @file
 *
 * Configuration defaults for EFI
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#define UACCESS_FLAT
#define IOMAP_VIRT
#define PCIAPI_EFI
#define DMAAPI_OP
#define CONSOLE_EFI
#define TIMER_EFI
#define UMALLOC_EFI
#define MEMMAP_NULL
#define SMBIOS_EFI
#define SANBOOT_EFI
#define BOFM_EFI
#define ENTROPY_EFITICK
#define ENTROPY_EFIRNG
#define TIME_EFI
#define REBOOT_EFI
#define ACPI_EFI
#define FDT_EFI
#define MPAPI_EFI
#define NAP_EFI

#define	NET_PROTO_IPV6		/* IPv6 protocol */
#define	NET_PROTO_LLDP		/* Link Layer Discovery protocol */

#define DOWNLOAD_PROTO_FILE	/* Local filesystem access */

#define	IMAGE_EFI		/* EFI image support */
#define	IMAGE_SCRIPT		/* iPXE script image support */
#define IMAGE_EFISIG		/* EFI signature list support */

#define	SANBOOT_PROTO_ISCSI	/* iSCSI protocol */
#define	SANBOOT_PROTO_AOE	/* AoE protocol */
#define	SANBOOT_PROTO_IB_SRP	/* Infiniband SCSI RDMA protocol */
#define	SANBOOT_PROTO_FCP	/* Fibre Channel protocol */
#define	SANBOOT_PROTO_HTTP	/* HTTP SAN protocol */

#define	USB_HCD_XHCI		/* xHCI USB host controller */
#define	USB_HCD_EHCI		/* EHCI USB host controller */
#define	USB_HCD_UHCI		/* UHCI USB host controller */
#define	USB_EFI			/* Provide EFI_USB_IO_PROTOCOL interface */
#define USB_BLOCK		/* USB block devices */

#define	REBOOT_CMD		/* Reboot command */

#define EFI_SETTINGS		/* EFI variable settings */

#define CERTS_EFI		/* EFI certificate sources */

#if defined ( __i386__ ) || defined ( __x86_64__ )
#define IOAPI_X86
#define ENTROPY_RDRAND
#define	CPUID_CMD		/* x86 CPU feature detection command */
#define	UNSAFE_STD		/* Avoid setting direction flag */
#endif

#if defined ( __arm__ ) || defined ( __aarch64__ )
#define IOAPI_ARM
#define FDT_CMD
#endif

#if defined ( __aarch64__ )
#define	IMAGE_GZIP		/* GZIP image support */
#define FDT_CMD
#endif

#if defined ( __loongarch__ )
#define IOAPI_LOONG64
#endif

#if defined ( __riscv )
#define IOAPI_RISCV
#define FDT_CMD
#endif

#endif /* CONFIG_DEFAULTS_EFI_H */
