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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ipxe/init.h>
#include <ipxe/dhcp.h>
#include <ipxe/settings.h>
#include <ipxe/uart.h>
#include <ipxe/console.h>
#include <ipxe/serial.h>
#include <config/console.h>
#include <config/serial.h>

/* Set default console usage if applicable */
#if ! ( defined ( CONSOLE_SERIAL ) && CONSOLE_EXPLICIT ( CONSOLE_SERIAL ) )
#undef CONSOLE_SERIAL
#define CONSOLE_SERIAL ( CONSOLE_USAGE_ALL & ~CONSOLE_USAGE_LOG )
#endif

/* UART port number */
#ifdef COMCONSOLE
#define CONSOLE_PORT COMCONSOLE
#else
#define CONSOLE_PORT 0
#endif

/** Serial console UART */
struct uart serial_console;

/**
 * Print a character to serial console
 *
 * @v character		Character to be printed
 */
static void serial_putchar ( int character ) {

	/* Do nothing if we have no UART */
	if ( ! serial_console.base )
		return;

	/* Transmit character */
	uart_transmit ( &serial_console, character );
}

/**
 * Get character from serial console
 *
 * @ret character	Character read from console
 */
static int serial_getchar ( void ) {
	uint8_t data;

	/* Do nothing if we have no UART */
	if ( ! serial_console.base )
		return 0;

	/* Wait for data to be ready */
	while ( ! uart_data_ready ( &serial_console ) ) {}

	/* Receive data */
	data = uart_receive ( &serial_console );

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
	if ( ! serial_console.base )
		return 0;

	/* Check UART */
	return uart_data_ready ( &serial_console );
}

/** Serial console */
struct console_driver serial_console_driver __console_driver = {
	.putchar = serial_putchar,
	.getchar = serial_getchar,
	.iskey = serial_iskey,
	.usage = CONSOLE_SERIAL,
};

/**
 * Shut down serial console
 *
 * @v flags		Shutdown flags
 */
static void serial_shutdown ( int flags __unused ) {

	/* Do nothing if we have no UART */
	if ( ! serial_console.base )
		return;

	/* Flush any pending output */
	uart_flush ( &serial_console );

	/* Leave console enabled; it's still usable */
}

/** Serial console startup function */
struct startup_fn serial_startup_fn __startup_fn ( STARTUP_EARLY ) = {
	.name = "serial",
	.shutdown = serial_shutdown,
};

/** Serial console port setting */
const struct setting serial_port_setting __setting ( SETTING_MISC, serial-port ) = {
	.name = "serial-port",
	.description = "Serial port",
	.tag = DHCP_EB_SERIAL_PORT,
	.type = &setting_type_uint8,
};
const struct setting serial_options_setting __setting ( SETTING_MISC, serial-options ) = {
	.name = "serial-options",
	.description = "Serial port options",
	.tag = DHCP_EB_SERIAL_OPTIONS,
	.type = &setting_type_string,
};

/**
 * Parse a string with options in the format of BBBBPDS where:
 * - BBBB is the baud rate
 * - P is parity ("n" for none, "o" for odd, "e" for even)
 * - D is data bits
 * - S is stop bits
 *
 * @c rc 		Return status code
 * @v baud		Pointer to an integer for baud rate
 * @v lcr		Pointer to an integer for the line control register
 */
static int serial_parse_options ( const char *options,
				   uint32_t *baud, uint8_t *lcr ) {
	int rc = 0;
	char *endp;
	uint32_t speed;
	uint8_t data, parity, stop, preserve;

	/* Initialise to default values; any of these can be overriden later */
	speed = COMSPEED;
	data = COMDATA;
	parity = COMPARITY;
	stop = COMSTOP;
	preserve = COMPRESERVE;

	if ( !options || strlen ( options ) == 0 )
		goto out;

	if ( strcmp ( options, "preserve" ) == 0 ) {
		preserve = 1;
		goto out;
	}

	speed = strtoul( options, &endp, 10 );

	/* No baud rate found */
	if ( endp == options ) {
		rc = -EINVAL;
		goto out;
	}

	/* Next character is parity */
	if ( *endp ) {
		switch( *endp++ ) {
		case 'n':
			parity = 0;
			break;
		case 'o':
			parity = 1;
			break;
		case 'e':
			parity = 3;
			break;
		default:
			rc = -EINVAL;
			goto out;
		}
	}

	/* Data & stop bits follow, single numbers in ASCII */
	if ( *endp )
		data = *endp++ - '0';
	if ( *endp )
		stop = *endp++ - '0';

 out:
	if ( !preserve ) {
		/* UART line control register */
		*baud = speed;
		*lcr = UART_LCR_WPS ( data, parity, stop );
	} else {
		/* Preserve the settings set by e.g. a previous bootloader */
		*baud = 0;
		*lcr = 0;
	}
	return rc;
}

/**
 * Apply serial console settings
 *
 * @ret rc	      Return status code
 */
static int apply_serial_settings ( void ) {
	int rc = 0;
	char *options;
	unsigned long port;
	uint32_t baud;
	uint8_t lcr;

	static unsigned long old_port;
	static uint32_t old_baud;
	static uint8_t old_lcr;

	if ( fetch_uint_setting ( NULL, &serial_port_setting, &port ) < 0 )
		port = CONSOLE_PORT;

	fetch_string_setting_copy ( NULL, &serial_options_setting, &options );
	if ( ( rc = serial_parse_options ( options, &baud, &lcr ) ) != 0 ) {
		goto err_parse_options;
	}

	/* Avoid reconfiguring the port if no changes are being made */
	if ( port == old_port && baud == old_baud && lcr == old_lcr )
		goto out_no_change;

	/* Flush the old port, if configured */
	if ( serial_console.base )
		uart_flush ( &serial_console );

	/* Disable port if we are not about to configure a new one */
	if ( ! port ) {
		serial_console.base = NULL;
		goto out_no_port;
	}

	/* Select UART */
	if ( ( rc = uart_select ( &serial_console, port ) ) != 0 ) {
		DBG ( "Could not select UART %ld: %s\n", port, strerror ( rc ) );
		goto err_port_select;
	}

	/* Initialise UART */
	if ( ( rc = uart_init ( &serial_console, baud, lcr ) ) != 0 ) {
		DBG ( "Could not initialise UART %ld baud %u LCR %#02x: %s\n",
		      port, baud, lcr, strerror ( rc ));
		goto err_port_init;
	}

	DBG ( "Serial config using port %ld\n", port );

	/* Record settings */
	old_port = port;
	old_baud = baud;
	old_lcr = lcr;

	/* Success */
	rc = 0;

 err_parse_options:
 err_port_select:
 err_port_init:
 out_no_port:
 out_no_change:
	free ( options );
	return rc;
}

/** Serial console port settings applicator */
struct settings_applicator serial_port_applicator __settings_applicator = {
	.apply = apply_serial_settings,
};
