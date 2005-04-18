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

#define CONSOLE_FIRMWARE	1	/* Default BIOS console */
#define CONSOLE_SERIAL		0	/* Serial port */
#define CONSOLE_DIRECT_VGA	0	/* Direct access to VGA card */
#define CONSOLE_BTEXT		0	/* Who knows what this does? */
#define CONSOLE_PC_KBD		0	/* Direct access to PC keyboard */

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

#define COMCONSOLE	0x3f8		/* I/O port address */

/* Keep settings from a previous user of the serial port (e.g. lilo or
 * LinuxBIOS), ignoring COMSPEED, COMDATA, COMPARITY and COMSTOP.
 */
#define COMPRESERVE	0

#if ! COMPRESERVE
#define COMSPEED	9600		/* Baud rate */
#define COMDATA		8		/* Data bits */ 
#define COMPARITY	0		/* Parity: 0=None, 1=Odd, 2=Even */
#define COMSTOP		1		/* Stop bits */
#endif

/* @END serial.h */
