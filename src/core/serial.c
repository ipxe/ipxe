/*
 * Copyright (C) 2014 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * Serial console
 *
 */

#include <stddef.h>
#include <string.h>
#include <ipxe/init.h>
#include <ipxe/uart.h>
#include <ipxe/console.h>
#include <ipxe/serial.h>
#include <ipxe/ns16550.h>
#include <config/console.h>
#include <config/serial.h>

/* Set default console usage if applicable */
#if ! ( defined ( CONSOLE_SERIAL ) && CONSOLE_EXPLICIT ( CONSOLE_SERIAL ) )
#undef CONSOLE_SERIAL
#define CONSOLE_SERIAL ( CONSOLE_USAGE_ALL & ~CONSOLE_USAGE_LOG )
#endif

#ifdef SERIAL_FIXED
#define SERIAL_PREFIX_fixed
#else
#define SERIAL_PREFIX_fixed __fixed_
#endif

/* Serial console UART */
#ifndef COMCONSOLE
#define COMCONSOLE NULL
#endif

/* Serial console baud rate */
#ifndef COMSPEED
#define COMSPEED 0
#endif

/** Active serial console UART
 *
 * Explicitly initialised to @c NULL since this variable may be
 * accessed before .bss has been zeroed.
 */
struct uart *serial_console = NULL;

/**
 * Get fixed serial console UART
 *
 * @ret uart		Serial console UART, or NULL
 */
static struct uart * serial_comconsole ( void ) {

	return COMCONSOLE;
}

/**
 * Print a character to serial console
 *
 * @v character		Character to be printed
 */
static void serial_putchar ( int character ) {

	/* Do nothing if we have no UART */
	if ( ! serial_console )
		return;

	/* Transmit character */
	uart_transmit ( serial_console, character );
}

/**
 * Get character from serial console
 *
 * @ret character	Character read from console
 */
static int serial_getchar ( void ) {
	uint8_t data;

	/* Do nothing if we have no UART */
	if ( ! serial_console )
		return 0;

	/* Wait for data to be ready */
	while ( ! uart_data_ready ( serial_console ) ) {}

	/* Receive data */
	data = uart_receive ( serial_console );

	/* Strip any high bit and convert DEL to backspace */
	data &= 0x7f;
	if ( data == 0x7f )
		data = 0x08;

	return data;
}

/**
 * Check for character ready to read from serial console
 *
 * @ret True		Character available to read
 * @ret False		No character available to read
 */
static int serial_iskey ( void ) {

	/* Do nothing if we have no UART */
	if ( ! serial_console )
		return 0;

	/* Check UART */
	return uart_data_ready ( serial_console );
}

/** Serial console */
struct console_driver serial_console_driver __console_driver = {
	.putchar = serial_putchar,
	.getchar = serial_getchar,
	.iskey = serial_iskey,
	.usage = CONSOLE_SERIAL,
};

/** Initialise serial console */
static void serial_init ( void ) {
	struct uart *uart;
	int rc;

	/* Get default serial console, if any */
	uart = default_serial_console();
	if ( ! uart )
		return;

	/* Initialise UART */
	if ( ( rc = uart_init ( uart, COMSPEED ) ) != 0 ) {
		DBGC ( uart, "SERIAL could not initialise %s baud %d: %s\n",
		       uart->name, COMSPEED, strerror ( rc ) );
		return;
	}

	/* Record UART as serial console */
	serial_console = uart;
	DBGC ( uart, "SERIAL using %s\n", uart->name );
}

/**
 * Shut down serial console
 *
 * @v flags		Shutdown flags
 */
static void serial_shutdown ( int flags __unused ) {

	/* Do nothing if we have no UART */
	if ( ! serial_console )
		return;

	/* Flush any pending output */
	uart_flush ( serial_console );

	/* Leave console enabled; it's still usable */
}

/** Serial console initialisation function */
struct init_fn serial_console_init_fn __init_fn ( INIT_CONSOLE ) = {
	.name = "serial",
	.initialise = serial_init,
};

/** Serial console startup function */
struct startup_fn serial_startup_fn __startup_fn ( STARTUP_EARLY ) = {
	.name = "serial",
	.shutdown = serial_shutdown,
};

PROVIDE_SERIAL_INLINE ( null, default_serial_console );
PROVIDE_SERIAL ( fixed, default_serial_console, serial_comconsole );
