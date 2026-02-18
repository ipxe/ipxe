#ifndef CONFIG_SETTINGS_H
#define CONFIG_SETTINGS_H

/** @file
 *
 * Configuration settings sources
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <config/defaults.h>

/* Settings sources supported on all platforms */
#define ACPI_SETTINGS		/* ACPI settings */
#define PCI_SETTINGS		/* PCI device settings */
#define USB_SETTINGS		/* USB device settings */

/* Settings sources supported only on EFI platforms */
#if defined ( PLATFORM_efi )
  #define EFI_SETTINGS		/* EFI variable settings */
#endif

/* Settings sources supported only when memory maps are available */
#if ! defined ( MEMMAP_NULL )
  //#define MEMMAP_SETTINGS	/* Memory map settings */
#endif

/* Settings sources supported only on x86 CPUs */
#if defined ( __i386__ ) || defined ( __x86_64__ )
  #define CPUID_SETTINGS	/* CPUID settings */
  //#define VMWARE_SETTINGS	/* VMware GuestInfo settings */
  //#define VRAM_SETTINGS	/* Video RAM dump settings */
#endif

/* Disable settings sources not historically included in BIOS builds */
#if defined ( PLATFORM_pcbios )
  #undef ACPI_SETTINGS
  #undef CPUID_SETTINGS
#endif

#include <config/named.h>
#include NAMED_CONFIG(settings.h)
#include <config/local/settings.h>
#include LOCAL_NAMED_CONFIG(settings.h)

#endif /* CONFIG_SETTINGS_H */
