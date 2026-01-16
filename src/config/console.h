#ifndef CONFIG_CONSOLE_H
#define CONFIG_CONSOLE_H

/** @file
 *
 * Console configuration
 *
 * These options specify the console types that iPXE will use for
 * interaction with the user.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <config/defaults.h>

/*****************************************************************************
 *
 * Console types
 *
 */

/* Console types supported on all platforms */
#define CONSOLE_FRAMEBUFFER	/* Graphical framebuffer console */
#define CONSOLE_SYSLOG		/* Syslog console */
#define CONSOLE_SYSLOGS		/* Encrypted syslog console */

/* Console types supported only on systems with serial ports */
#if ! defined ( SERIAL_NULL )
  //#define CONSOLE_SERIAL	/* Serial port console */
#endif

/* Console types supported only on BIOS platforms */
#if defined ( PLATFORM_pcbios )
  //#define CONSOLE_INT13	/* INT13 disk log console */
  #define CONSOLE_PCBIOS	/* Default BIOS console */
#endif

/* Console types supported only on EFI platforms */
#if defined ( PLATFORM_efi )
  #define CONSOLE_EFI		/* Default EFI console */
#endif

/* Console types supported only on RISC-V SBI platforms */
#if defined ( PLATFORM_sbi )
  #define CONSOLE_SBI		/* RISC-V SBI debug console */
#endif

/* Console types supported only on Linux platforms */
#if defined ( PLATFORM_linux )
  #define CONSOLE_LINUX		/* Default Linux console */
#endif

/* Console types supported only on x86 CPUs */
#if defined ( __i386__ ) || defined ( __x86_64__ )
  //#define CONSOLE_DEBUGCON	/* Bochs/QEMU/KVM debug port console */
  //#define CONSOLE_DIRECT_VGA	/* Direct access to VGA card */
  //#define CONSOLE_PC_KBD	/* Direct access to PC keyboard */
  //#define CONSOLE_VMWARE	/* VMware logfile console */
#endif

/* Enable serial console on platforms that are typically headless */
#if defined ( CONSOLE_SBI )
  #define CONSOLE_SERIAL
#endif

/* Disable console types not historically included in BIOS builds */
#if defined ( PLATFORM_pcbios )
  #undef CONSOLE_FRAMEBUFFER
  #undef CONSOLE_SYSLOG
  #undef CONSOLE_SYSLOGS
#endif

/*****************************************************************************
 *
 * Keyboard maps
 *
 * See hci/keymap/keymap_*.c for available keyboard maps.
 *
 */

#define KEYBOARD_MAP	us	/* Default US keyboard map */
//#define KEYBOARD_MAP	dynamic	/* Runtime selectable keyboard map */

/*****************************************************************************
 *
 * Log levels
 *
 * Control which syslog() messages are generated.  Note that this is
 * not related in any way to CONSOLE_SYSLOG.
 *
 */

#define LOG_LEVEL	LOG_NONE

#include <config/named.h>
#include NAMED_CONFIG(console.h)
#include <config/local/console.h>
#include LOCAL_NAMED_CONFIG(console.h)

#endif /* CONFIG_CONSOLE_H */
