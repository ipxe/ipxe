/*
 * This file defines the configuration for Etherboot.
 *
 * The build system splits this file into several individual header
 * files of the form config/%.h, so that changing one option doesn't
 * necessitate a rebuild of every single object.  For this reason, it
 * is important to maintain the strict formatting in this file.
 *
 */

/* @BEGIN general.h
 *
 * Console configuration
 *
 * These options specify the console types that Etherboot will use for
 * interaction with the user.
 *
 */

#define	CONSOLE_FIRMWARE	/* Default BIOS console */
#undef	CONSOLE_SERIAL		/* Serial port */
#undef	CONSOLE_DIRECT_VGA	/* Direct access to VGA card */
#undef	CONSOLE_BTEXT		/* Who knows what this does? */
#undef	CONSOLE_PC_KBD		/* Direct access to PC keyboard */

/* @END general.h */

/* @BEGIN serial.h
 *
 * Serial port configuration
 *
 * These options affect the operation of the serial console.  They
 * take effect only if the serial console is included using the
 * CONSOLE_SERIAL option.
 *
 */

#define	COMCONSOLE	0x3f8		/* I/O port address */

/* Keep settings from a previous user of the serial port (e.g. lilo or
 * LinuxBIOS), ignoring COMSPEED, COMDATA, COMPARITY and COMSTOP.
 */
#undef	COMPRESERVE

#ifndef COMPRESERVE
#define	COMSPEED	115200		/* Baud rate */
#define	COMDATA		8		/* Data bits */ 
#define	COMPARITY	0		/* Parity: 0=None, 1=Odd, 2=Even */
#define	COMSTOP		1		/* Stop bits */
#endif

/* @END serial.h */

/* @BEGIN isa.h
 *
 * ISA probe address configuration
 *
 * You can override the list of addresses that will be probed by any
 * ISA drivers.
 *
 */
#undef	ISA_PROBE_ADDRS		/* e.g. 0x200, 0x300 */
#undef	ISA_PROBE_ONLY		/* Do not probe any other addresses */

/* @END isa.h */

/* @BEGIN general.h
 *
 * Download protocols
 *
 */

#define	DOWNLOAD_PROTO_TFTP	/* Trivial File Transfer Protocol */
#undef	DOWNLOAD_PROTO_NFS	/* Network File System */
#undef	DOWNLOAD_PROTO_HTTP	/* Hypertext Transfer Protocol */
#undef	DOWNLOAD_PROTO_TFTM	/* Multicast Trivial File Transfer Protocol */
#undef	DOWNLOAD_PROTO_SLAM	/* Scalable Local Area Multicast */
#undef	DOWNLOAD_PROTO_FSP	/* FSP? */

/* @END general.h */

/* @BEGIN general.h
 *
 * Name resolution modules
 *
 */

#undef	DNS_RESOLVER		/* DNS resolver */
#undef	NMB_RESOLVER		/* NMB resolver */

/* @END general.h */

/* @BEGIN general.h
 *
 * Image types
 *
 * Etherboot supports various image formats.  Select whichever ones
 * you want to use.
 *
 */
#undef	TAGGED_IMAGE		/* NBI image support */
#undef	ELF64_IMAGE		/* ELF64 image support */
#undef	ELF_IMAGE		/* ELF image support */
#undef	COFF_IMAGE		/* COFF image support */
#undef	IMAGE_FREEBSD		/* FreeBSD kernel image support */
#undef	IMAGE_MULTIBOOT		/* MultiBoot image support */
#undef	AOUT_IMAGE		/* a.out image support */
#undef	WINCE_IMAGE		/* WinCE image support */
#undef	PXE_IMAGE		/* PXE image support */

/* @END general.h */ 

/* @BEGIN general.h
 *
 * Command-line commands to include
 *
 */
#define	BOOT_CMD		/* Automatic booting */
#define	NVO_CMD			/* Non-volatile option storage commands */
#define	CONFIG_CMD		/* Option configuration console */

/* @END general.h */ 

/* @BEGIN general.h
 *
 * Obscure configuration options
 *
 * You probably don't need to touch these.
 *
 */

#undef	BUILD_SERIAL		/* Include an automatic build serial
				 * number.  Add "bs" to the list of
				 * make targets.  For example:
				 * "make bin/rtl8139.dsk bs" */
#undef	BUILD_ID		/* Include a custom build ID string,
				 * e.g "test-foo" */
#undef	NULL_TRAP		/* Attempt to catch NULL function calls */

/* @END general.h */
