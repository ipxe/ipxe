/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Generic UARTs
 *
 */

#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <ipxe/uart.h>

/** List of registered UARTs */
LIST_HEAD ( uarts );

static void null_uart_transmit ( struct uart *uart __unused,
				 uint8_t byte __unused ) {
}

static int null_uart_data_ready ( struct uart *uart __unused ) {
	return 0;
}

static uint8_t null_uart_receive ( struct uart *uart __unused ) {
	return 0;
}

static int null_uart_init ( struct uart *uart __unused,
			    unsigned int baud __unused ) {
	return 0;
}

static void null_uart_flush ( struct uart *uart __unused ) {
}

/** Null UART operations */
struct uart_operations null_uart_operations = {
	.transmit = null_uart_transmit,
	.data_ready = null_uart_data_ready,
	.receive = null_uart_receive,
	.init = null_uart_init,
	.flush = null_uart_flush,
};

/**
 * Allocate UART
 *
 * @v priv_len		Length of private data
 * @ret uart		UART, or NULL on error
 */
struct uart * alloc_uart ( size_t priv_len ) {
	struct uart *uart;

	/* Allocate and initialise UART */
	uart = zalloc ( sizeof ( *uart ) + priv_len );
	if ( ! uart )
		return NULL;
	uart->priv = ( ( ( void * ) uart ) + sizeof ( *uart ) );

	return uart;
}

/**
 * Register fixed UARTs (when not provided by platform)
 *
 * @ret rc		Return status code
 */
__weak int uart_register_fixed ( void ) {

	return 0;
}

/**
 * Register UART
 *
 * @v uart		UART
 * @ret rc		Return status code
 */
int uart_register ( struct uart *uart ) {

	/* Add to list of registered UARTs */
	uart_get ( uart );
	list_add_tail ( &uart->list, &uarts );
	DBGC ( uart, "UART %s registered\n", uart->name );

	return 0;
}

/**
 * Unregister UART
 *
 * @v uart		UART
 */
void uart_unregister ( struct uart *uart ) {

	/* Remove from list of registered UARTs */
	list_del ( &uart->list );
	uart_put ( uart );
}

/**
 * Find named UART
 *
 * @v name		UART name
 * @ret uart		UART, or NULL if not found
 */
struct uart * uart_find ( const char *name ) {
	struct uart *uart;
	unsigned int index;
	char *endp;
	int rc;

	/* Register fixed platform UARTs if not already registered */
	if ( list_empty ( &uarts ) ) {
		if ( ( rc = uart_register_fixed() ) != 0 ) {
			DBGC ( &uarts, "UART could not register fixed UARTs: "
			       "%s\n", strerror ( rc ) );
			/* Continue anyway */
		}
	}

	/* Try parsing name as a numeric index */
	index = strtoul ( name, &endp, 10 );

	/* Find matching UART, if any */
	list_for_each_entry ( uart, &uarts, list ) {

		/* Check for a matching name */
		if ( strcasecmp ( name, uart->name ) == 0 )
			return uart;

		/* Check for a matching numeric index */
		if ( ( *endp == '\0' ) && ( index-- == 0 ) )
			return uart;
	}

	DBGC ( &uarts, "UART %s not found\n", name );
	return NULL;
}
