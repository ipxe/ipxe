/*
 * Copyright (C) 2008 Stefan Hajnoczi <stefanha@gmail.com>.
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

#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <ipxe/uart.h>
#include <ipxe/gdbstub.h>
#include <ipxe/gdbserial.h>
#include <config/serial.h>

/* UART baud rate */
#ifdef COMPRESERVE
#define GDBSERIAL_BAUD 0
#else
#define GDBSERIAL_BAUD COMSPEED
#endif

/** GDB serial UART */
static struct uart *gdbserial_uart;

struct gdb_transport serial_gdb_transport __gdb_transport;

static size_t gdbserial_recv ( char *buf, size_t len ) {

	assert ( len > 0 );
	while ( ! uart_data_ready ( gdbserial_uart ) ) {}
	buf[0] = uart_receive ( gdbserial_uart );
	return 1;
}

static void gdbserial_send ( const char *buf, size_t len ) {

	while ( len-- > 0 ) {
		uart_transmit ( gdbserial_uart, *buf++ );
	}
}

static int gdbserial_init ( int argc, char **argv ) {
	const char *port;

	if ( argc == 1 ) {
		port = argv[0];
	} else {
		printf ( "serial: syntax <port>\n" );
		return 1;
	}

	if ( ! gdbserial_configure ( port, GDBSERIAL_BAUD ) ) {
		printf ( "serial: unable to configure\n" );
		return 1;
	}

	return 0;
}

struct gdb_transport serial_gdb_transport __gdb_transport = {
	.name = "serial",
	.init = gdbserial_init,
	.recv = gdbserial_recv,
	.send = gdbserial_send,
};

struct gdb_transport * gdbserial_configure ( const char *name,
					     unsigned int baud ) {
	int rc;

	uart_put ( gdbserial_uart );
	gdbserial_uart = NULL;

	gdbserial_uart = uart_find ( name );
	if ( ! gdbserial_uart )
		return NULL;
	uart_get ( gdbserial_uart );

	if ( ( rc = uart_init ( gdbserial_uart, baud ) ) != 0 )
		return NULL;

	return &serial_gdb_transport;
}
