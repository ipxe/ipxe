#ifndef CONFIG_DEFAULTS_SBI_H
#define CONFIG_DEFAULTS_SBI_H

/** @file
 *
 * Configuration defaults for RISC-V SBI
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#define IOAPI_RISCV
#define IOMAP_VIRT
#define DMAAPI_FLAT
#define UACCESS_OFFSET
#define TIMER_ZICNTR
#define ENTROPY_ZKR

#define CONSOLE_SBI
#define REBOOT_SBI
#define UMALLOC_SBI
#define MEMMAP_FDT

#define ACPI_NULL
#define MPAPI_NULL
#define NAP_NULL
#define PCIAPI_NULL
#define SANBOOT_NULL
#define SMBIOS_NULL
#define TIME_NULL

#define IMAGE_SCRIPT

#define REBOOT_CMD
#define POWEROFF_CMD
#define FDT_CMD

#endif /* CONFIG_DEFAULTS_SBI_H */
