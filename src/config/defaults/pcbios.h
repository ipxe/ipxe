#ifndef CONFIG_DEFAULTS_PCBIOS_H
#define CONFIG_DEFAULTS_PCBIOS_H

/** @file
 *
 * Configuration defaults for PCBIOS
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#define UACCESS_OFFSET
#define IOAPI_X86
#define PCIAPI_PCBIOS
#define DMAAPI_FLAT
#define TIMER_PCBIOS
#define CONSOLE_PCBIOS
#define NAP_PCBIOS
#define UMALLOC_UHEAP
#define MEMMAP_INT15
#define SMBIOS_PCBIOS
#define SANBOOT_PCBIOS
#define ENTROPY_RTC
#define ENTROPY_RDRAND
#define TIME_RTC
#define REBOOT_PCBIOS
#define ACPI_RSDP
#define MPAPI_PCBIOS

#ifdef __x86_64__
#define IOMAP_PAGES
#else
#define IOMAP_VIRT
#endif

#define	IMAGE_ELF		/* ELF image support */
#define	IMAGE_MULTIBOOT		/* MultiBoot image support */
#define	IMAGE_PXE		/* PXE image support */
#define IMAGE_SCRIPT		/* iPXE script image support */
#define IMAGE_BZIMAGE		/* Linux bzImage image support */

#define PXE_STACK		/* PXE stack in iPXE - required for PXELINUX */
#define PXE_MENU		/* PXE menu booting */

#define	SANBOOT_PROTO_ISCSI	/* iSCSI protocol */
#define	SANBOOT_PROTO_AOE	/* AoE protocol */
#define	SANBOOT_PROTO_IB_SRP	/* Infiniband SCSI RDMA protocol */
#define	SANBOOT_PROTO_FCP	/* Fibre Channel protocol */
#define SANBOOT_PROTO_HTTP	/* HTTP SAN protocol */

#define	USB_HCD_XHCI		/* xHCI USB host controller */
#define	USB_HCD_EHCI		/* EHCI USB host controller */
#define	USB_HCD_UHCI		/* UHCI USB host controller */
#define	USB_KEYBOARD		/* USB keyboards */
#define USB_BLOCK		/* USB block devices */

#define	REBOOT_CMD		/* Reboot command */
#define	CPUID_CMD		/* x86 CPU feature detection command */

#endif /* CONFIG_DEFAULTS_PCBIOS_H */
