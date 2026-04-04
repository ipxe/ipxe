/*
 * Copyright (C) 2026 Daniel Wagner <wagi@monom.org>.
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
 * Dumb serial console
 *
 * Strips ANSI escape sequences from serial console output.  Useful
 * for environments (e.g. LAVA) that cannot handle escape sequences.
 *
 */

#include <ipxe/uart.h>
#include <ipxe/ansiesc.h>
#include <ipxe/console.h>
#include <ipxe/serial.h>
#include <ipxe/init.h>

/** Serial console driver */
extern struct console_driver serial_console_driver;

/** Dumb serial console ANSI escape sequence handlers */
static struct ansiesc_handler serial_dumb_ansiesc_handlers[] = {
	{ 0, NULL }
};

/** Dumb serial console ANSI escape sequence context */
static struct ansiesc_context serial_dumb_ansiesc_ctx = {
	.handlers = serial_dumb_ansiesc_handlers,
};

/**
 * Print a character to dumb serial console
 *
 * @v character		Character to be printed
 */
static void serial_dumb_putchar ( int character ) {

	/* Do nothing if we have no UART */
	if ( ! serial_console )
		return;

	/* Intercept ANSI escape sequences */
	character = ansiesc_process ( &serial_dumb_ansiesc_ctx, character );
	if ( character < 0 )
		return;

	/* Transmit character */
	uart_transmit ( serial_console, character );
}

/**
 * Initialise dumb serial console
 *
 * Replaces the default serial console putchar with a version that
 * strips ANSI escape sequences before transmitting.  This runs at
 * INIT_EARLY priority, before any console output takes place.
 */
static void serial_dumb_init ( void ) {
	serial_console_driver.putchar = serial_dumb_putchar;
}

/** Dumb serial console initialisation function */
struct init_fn serial_dumb_init_fn __init_fn ( INIT_EARLY ) = {
	.name = "serial_dumb",
	.initialise = serial_dumb_init,
};
