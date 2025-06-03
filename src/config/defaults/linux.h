#ifndef CONFIG_DEFAULTS_LINUX_H
#define CONFIG_DEFAULTS_LINUX_H

/** @file
 *
 * Configuration defaults for linux
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#define CONSOLE_LINUX
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

#define DRIVERS_LINUX

#define IMAGE_SCRIPT

#define SANBOOT_PROTO_ISCSI
#define SANBOOT_PROTO_AOE
#define SANBOOT_PROTO_IB_SRP
#define SANBOOT_PROTO_FCP
#define SANBOOT_PROTO_HTTP

#if defined ( __i386__ ) || defined ( __x86_64__ )
#define ENTROPY_RDRAND
#endif

#endif /* CONFIG_DEFAULTS_LINUX_H */
