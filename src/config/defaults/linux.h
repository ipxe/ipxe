#ifndef CONFIG_DEFAULTS_LINUX_H
#define CONFIG_DEFAULTS_LINUX_H

/** @file
 *
 * Configuration defaults for linux
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#define TIMER_LINUX
#define UACCESS_LINUX
#define UMALLOC_LINUX
#define MEMMAP_NULL
#define NAP_LINUX
#define SMBIOS_LINUX
#define SANBOOT_DUMMY
#define ENTROPY_LINUX
#define TIME_LINUX
#define REBOOT_NULL
#define PCIAPI_LINUX
#define DMAAPI_FLAT
#define ACPI_LINUX
#define MPAPI_NULL
#define SERIAL_NULL
#define FDT_NULL

#define DRIVERS_LINUX

#if defined ( __i386__ ) || defined ( __x86_64__ )
#define ENTROPY_RDRAND
#endif

#endif /* CONFIG_DEFAULTS_LINUX_H */
