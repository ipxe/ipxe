#ifndef CONFIG_DEFAULTS_PCBIOS_H
#define CONFIG_DEFAULTS_PCBIOS_H

/** @file
 *
 * Configuration defaults for PCBIOS
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#define UACCESS_LIBRM
#define IOAPI_X86
#define PCIAPI_PCBIOS
#define TIMER_PCBIOS
#define CONSOLE_PCBIOS
#define NAP_PCBIOS
#define UMALLOC_MEMTOP
#define SMBIOS_PCBIOS

#define	IMAGE_ELF		/* ELF image support */
#define	IMAGE_MULTIBOOT		/* MultiBoot image support */
#define	IMAGE_PXE		/* PXE image support */
#define IMAGE_SCRIPT		/* gPXE script image support */
#define IMAGE_BZIMAGE		/* Linux bzImage image support */
#define IMAGE_COMBOOT		/* SYSLINUX COMBOOT image support */

#define PXE_STACK		/* PXE stack in gPXE - required for PXELINUX */
#define PXE_MENU		/* PXE menu booting */
#define	PXE_CMD			/* PXE commands */

#define	SANBOOT_PROTO_ISCSI	/* iSCSI protocol */
#define	SANBOOT_PROTO_AOE	/* AoE protocol */

#endif /* CONFIG_DEFAULTS_PCBIOS_H */
