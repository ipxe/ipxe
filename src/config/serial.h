#ifndef CONFIG_SERIAL_H
#define CONFIG_SERIAL_H

/** @file
 *
 * Serial port configuration
 *
 * These options affect the operation of the serial console. They take effect
 * only if the serial console is included using the CONSOLE_SERIAL option.
 * These settings only the default settings, and can all be set (or overriden)
 * at runtime.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

//#define COMCONSOLE	COM1	/* I/O port address */
#define COMPRESERVE	0	/* Preserve settings by another bootloader */
#define COMSPEED	115200	/* Baud rate */
#define COMDATA		8	/* Data bits */
#define COMPARITY	0	/* Parity: 0=None, 1=Odd, 2=Even */
#define COMSTOP		1	/* Stop bits */

#include <config/named.h>
#include NAMED_CONFIG(serial.h)
#include <config/local/serial.h>
#include LOCAL_NAMED_CONFIG(serial.h)

#endif /* CONFIG_SERIAL_H */
