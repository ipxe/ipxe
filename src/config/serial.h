#ifndef CONFIG_SERIAL_H
#define CONFIG_SERIAL_H

/** @file
 *
 * Serial port configuration
 *
 * These options affect the operation of the serial console.  They
 * take effect only if the serial console is included using the
 * CONSOLE_SERIAL option.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <config/defaults.h>

#define	COMCONSOLE	COM1		/* I/O port address */

/* Keep settings from a previous user of the serial port (e.g. lilo or
 * LinuxBIOS), ignoring COMSPEED.
 */
#undef	COMPRESERVE

#ifndef COMPRESERVE
#define	COMSPEED	115200		/* Baud rate */
#endif

/* Early UART configuration (for bare metal prefix debugging only) */
//#define EARLY_UART_MODEL	8250
//#define EARLY_UART_REG_BASE	0x10000000
//#define EARLY_UART_REG_SHIFT	0

#include <config/named.h>
#include NAMED_CONFIG(serial.h)
#include <config/local/serial.h>
#include LOCAL_NAMED_CONFIG(serial.h)

#endif /* CONFIG_SERIAL_H */
