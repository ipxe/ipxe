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

#ifdef COMPRESERVE
#define	COMSPEED	9600		/* Baud rate */
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
 * Name resolution modules
 *
 */

#define	DNS_RESOLVER		/* DNS resolver */
#define NMB_RESOLVER		/* NMB resolver */

/* @END general.h */

/* @BEGIN general.h
 *
 * Obscure configuration options
 *
 * You probably don't need to touch these.
 *
 */

#define	RELOCATE		/* Relocate to high memory */
#undef	BUILD_SERIAL		/* Include an automatic build serial
				 * number.  Add "bs" to the list of
				 * make targets.  For example:
				 * "make bin/rtl8139.dsk bs" */
#undef	BUILD_ID		/* Include a custom build ID string,
				 * e.g "test-foo" */

/* @END general.h */
