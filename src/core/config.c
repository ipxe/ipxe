/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#include "etherboot.h"
#include "dev.h"
#include "console.h"

#include "config/general.h"

/*
 * Build ID string calculations
 *
 */
#undef XSTR
#undef STR
#define XSTR(s) STR(s)
#define STR(s) #s

#ifdef BUILD_SERIAL
#include "config/.buildserial.h"
#define BUILD_SERIAL_STR " #" XSTR(BUILD_SERIAL_NUM)
#else
#define BUILD_SERIAL_STR ""
#endif

#ifdef BUILD_ID
#define BUILD_ID_STR " " BUILD_ID
#else
#define BUILD_ID_STR ""
#endif

#if defined(BUILD_ID) || defined(BUILD_SERIAL)
#define BUILD_STRING " [build" BUILD_ID_STR BUILD_SERIAL_STR "]"
#else
#define BUILD_STRING ""
#endif

/*
 * Drag in all requested console types
 *
 * CONSOLE_DUAL sets both CONSOLE_FIRMWARE and CONSOLE_SERIAL for
 * legacy compatibility.
 *
 */

#ifdef	CONSOLE_DUAL
#undef	CONSOLE_FIRMWARE
#define	CONSOLE_FIRMWARE	1
#undef	CONSOLE_SERIAL
#define	CONSOLE_SERIAL		1
#endif

#ifdef CONSOLE_FIRMWARE
REQUIRE_OBJECT ( bios_console );
#endif

#ifdef CONSOLE_SERIAL
REQUIRE_OBJECT ( serial );
#endif

#ifdef CONSOLE_DIRECT_VGA
REQUIRE_OBJECT ( video_subr );
#endif

#ifdef CONSOLE_BTEXT
REQUIRE_OBJECT ( btext );
#endif

#ifdef CONSOLE_PC_KBD
REQUIRE_OBJECT ( pc_kbd );
#endif

/*
 * Drag in all requested protocols
 *
 */
#ifdef DOWNLOAD_PROTO_TFTP
REQUIRE_OBJECT ( tftp );
#endif
#ifdef DOWNLOAD_PROTO_NFS
REQUIRE_OBJECT ( nfs );
#endif
#ifdef DOWNLOAD_PROTO_HTTP
REQUIRE_OBJECT ( http );
#endif
#ifdef DOWNLOAD_PROTO_TFTM
REQUIRE_OBJECT ( tftm );
#endif
#ifdef DOWNLOAD_PROTO_SLAM
REQUIRE_OBJECT ( slam );
#endif

/*
 * Drag in any required resolvers
 *
 */
#ifdef DNS_RESOLVER
REQUIRE_OBJECT ( dns );
#endif

#ifdef NMB_RESOLVER
REQUIRE_OBJECT ( nmb );
#endif

/*
 * Drag in all requested image formats
 *
 */
#ifdef IMAGE_NBI
REQUIRE_OBJECT ( nbi );
#endif
#ifdef IMAGE_ELF64
REQUIRE_OBJECT ( elf64 );
#endif
#ifdef IMAGE_ELF
REQUIRE_OBJECT ( elf );
#endif
#ifdef IMAGE_ELF
REQUIRE_OBJECT ( coff );
#endif
#ifdef IMAGE_FREEBSD
REQUIRE_OBJECT ( freebsd );
#endif
#ifdef IMAGE_MULTIBOOT
REQUIRE_OBJECT ( multiboot );
#endif
#ifdef IMAGE_AOUT
REQUIRE_OBJECT ( aout );
#endif
#ifdef IMAGE_WINCE
REQUIRE_OBJECT ( wince );
#endif
#ifdef IMAGE_PXE
REQUIRE_OBJECT ( pxe_image );
#endif
#ifdef IMAGE_SCRIPT
REQUIRE_OBJECT ( script );
#endif

/*
 * Drag in all requested commands
 *
 */
#ifdef AUTOBOOT_CMD
REQUIRE_OBJECT ( autoboot_cmd );
#endif
#ifdef NVO_CMD
REQUIRE_OBJECT ( nvo_cmd );
#endif
#ifdef CONFIG_CMD
REQUIRE_OBJECT ( config_cmd );
#endif
#ifdef IFMGMT_CMD
REQUIRE_OBJECT ( ifmgmt_cmd );
#endif
#ifdef ROUTE_CMD
REQUIRE_OBJECT ( route_cmd );
#endif
#ifdef IMAGE_CMD
REQUIRE_OBJECT ( image_cmd );
#endif
#ifdef DHCP_CMD
REQUIRE_OBJECT ( dhcp_cmd );
#endif

/*
 * Drag in miscellaneous objects
 *
 */
#ifdef	NULL_TRAP
REQUIRE_OBJECT ( nulltrap );
#endif
