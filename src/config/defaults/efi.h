#ifndef CONFIG_DEFAULTS_EFI_H
#define CONFIG_DEFAULTS_EFI_H

/** @file
 *
 * Configuration defaults for EFI
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#define UACCESS_FLAT
#define IOMAP_VIRT
#define PCIAPI_EFI
#define DMAAPI_OP
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
#define MPAPI_EFI
#define NAP_EFI
#define SERIAL_SPCR

#if defined ( __i386__ ) || defined ( __x86_64__ )
#define IOAPI_X86
#define ENTROPY_RDRAND
#define	UNSAFE_STD		/* Avoid setting direction flag */
#define FDT_NULL
#endif

#if defined ( __arm__ ) || defined ( __aarch64__ )
#define IOAPI_ARM
#define FDT_EFI
#endif

#if defined ( __loongarch__ )
#define IOAPI_LOONG64
#define FDT_EFI
#endif

#if defined ( __riscv )
#define IOAPI_RISCV
#define FDT_EFI
#endif

#endif /* CONFIG_DEFAULTS_EFI_H */
