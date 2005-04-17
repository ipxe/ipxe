/*
 * This file defines the configuration for Etherboot.
 *
 * The build system splits this file into several individual header
 * files of the form config/%.h, so that changing one option doesn't
 * necessitate a rebuild of every single object.  For this reason, it
 * is important to maintain the strict formatting in this file.
 *
 */

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
#define COMPARITY	N		/* Parity */
#define COMSTOP		1		/* Stop bits */
#endif

/* @END serial.h */
