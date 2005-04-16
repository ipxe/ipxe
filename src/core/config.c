/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#include "etherboot.h"
#include "dev.h"
#include "console.h"
#ifdef BUILD_SERIAL
#include ".buildserial.h"
#define xstr(s) str(s)
#define str(s) #s
#endif

void print_config ( void ) {
	printf( "Etherboot " VERSION
#ifdef BUILD_SERIAL
		" [build " 
#ifdef BUILD_ID
		BUILD_ID " "
#endif
		"#" xstr(BUILD_SERIAL_NUM) "]"
#endif /* BUILD_SERIAL */
		" (GPL) http://etherboot.org\n"
		"Drivers: " );
	print_drivers();
	printf( "  Images: " 
#ifdef	TAGGED_IMAGE
		"NBI "
#endif
#ifdef	ELF64_IMAGE
		"ELF64 "
#endif
#ifdef	ELF_IMAGE
		"ELF "
#endif
#ifdef	COFF_IMAGE
		"COFF "
#endif
#ifdef	IMAGE_FREEBSD
		"FreeBSD "
#endif
#ifdef	IMAGE_MULTIBOOT
		"Multiboot "
#endif
#ifdef	AOUT_IMAGE
		"a.out "
#endif
#ifdef	WINCE_IMAGE
		"WINCE "
#endif
#ifdef	PXE_IMAGE
		"PXE "
#endif
#ifdef PXE_EXPORT /* All possible exports */
		"  Exports: "
#ifdef PXE_EXPORT
		"PXE "
#endif
#endif /* All possible exports */
		"  "
		);
#if	(BOOTP_SERVER != 67) || (BOOTP_CLIENT != 68)
	printf( "[DHCP ports %d and %d] ",
		BOOTP_SERVER, BOOTP_CLIENT);
#endif
	putchar('\n');
	printf( "Protocols: "
#ifdef RARP_NOT_BOOTP
		"RARP "
#else
# ifndef NO_DHCP_SUPPORT
		"DHCP "
# else
		"BOOTP "
# endif
#endif
#ifdef DOWNLOAD_PROTO_TFTP
		"TFTP "
#endif
#ifdef  DOWNLOAD_PROTO_NFS
		"NFS "
#endif
#ifdef  DOWNLOAD_PROTO_SLAM
		"SLAM "
#endif
#ifdef  DOWNLOAD_PROTO_TFTM
		"TFTM "
#endif
#ifdef  DOWNLOAD_PROTO_HTTP
		"HTTP "
#endif
#ifdef  PROTO_LACP
		"LACP "
#endif
#ifdef DNS_RESOLVER
		"DNS "
#endif
		"\n");
#ifdef KEEP_IT_REAL
	printf( "Keeping It Real [EXPERIMENTAL]\n" );
#endif
}

/*
 * Drag in all requested console types
 *
 * At least one of the CONSOLE_xxx has to be set.  CONSOLE_DUAL sets
 * both CONSOLE_FIRMWARE and CONSOLE_SERIAL for legacy compatibility.
 * If no CONSOLE_xxx is set, CONSOLE_FIRMWARE is assumed.
 *
 */

#ifdef CONSOLE_CRT
#define CONSOLE_FIRMWARE
#endif

#ifdef	CONSOLE_DUAL
#undef CONSOLE_FIRMWARE
#define CONSOLE_FIRMWARE
#undef CONSOLE_SERIAL
#define CONSOLE_SERIAL
#endif

#if	!defined(CONSOLE_FIRMWARE) && !defined(CONSOLE_SERIAL)
#define CONSOLE_FIRMWARE
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
 * Drag in relocate.o if required
 *
 */

#ifndef NORELOCATE
REQUIRE_OBJECT ( relocate );
#endif

/*
 * Allow ISA probe address list to be overridden
 *
 */
#include "isa.h"
#ifndef ISA_PROBE_ADDRS
#define ISA_PROBE_ADDRS
#endif

isa_probe_addr_t isa_extra_probe_addrs[] = {
	ISA_PROBE_ADDRS
};

unsigned int isa_extra_probe_addr_count
      = sizeof ( isa_extra_probe_addrs ) / sizeof ( isa_extra_probe_addrs[0] );
