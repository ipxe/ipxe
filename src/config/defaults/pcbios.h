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
#define SERIAL_SPCR
#define FDT_NULL

#ifdef __x86_64__
#define IOMAP_PAGES
#else
#define IOMAP_VIRT
#endif

#endif /* CONFIG_DEFAULTS_PCBIOS_H */
