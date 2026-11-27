#ifndef CONFIG_DEFAULTS_UPL_H
#define CONFIG_DEFAULTS_UPL_H

/** @file
 *
 * Configuration defaults for Universal Payload (UPL) images
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#define UACCESS_FLAT
#define IOMAP_VIRT
#define IOAPI_X86
#define DMAAPI_FLAT
#define PCIAPI_CLOUD
#define PCIAPI_RUNTIME_ECAM
#define PCIAPI_RUNTIME_DIRECT
#define TIMER_RDTSC
#define TIMER_ACPI
#define UMALLOC_UHEAP
#define MEMMAP_FDT
#define SERIAL_UPL
#define ACPI_UPL
#define SMBIOS_UPL
#define REBOOT_UPL
#define ENTROPY_RDRAND
#define TIME_RTC

#define MPAPI_NULL
#define NAP_NULL
#define SANBOOT_NULL
#define FDT_NULL

#endif /* CONFIG_DEFAULTS_UPL_H */
