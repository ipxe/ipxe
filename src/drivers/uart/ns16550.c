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
 * 16550-compatible UART
 *
 */

#include <unistd.h>
#include <errno.h>
#include <ipxe/uart.h>
#include <ipxe/ns16550.h>

/** Timeout for transmit holding register to become empty */
#define NS16550_THRE_TIMEOUT_MS 100

/** Timeout for transmitter to become empty */
#define NS16550_TEMT_TIMEOUT_MS 1000

/**
 * Transmit data
 *
 * @v uart		UART
 * @v data		Data
 */
static void ns16550_transmit ( struct uart *uart, uint8_t data ) {
	struct ns16550_uart *ns16550 = uart->priv;
	unsigned int i;
	uint8_t lsr;

	/* Wait for transmitter holding register to become empty */
	for ( i = 0 ; i < NS16550_THRE_TIMEOUT_MS ; i++ ) {
		lsr = ns16550_read ( ns16550, NS16550_LSR );
		if ( lsr & NS16550_LSR_THRE )
			break;
		mdelay ( 1 );
	}

	/* Transmit data (even if we timed out) */
	ns16550_write ( ns16550, NS16550_THR, data );
}

/**
 * Check if data is ready
 *
 * @v uart		UART
 * @ret ready		Data is ready
 */
static int ns16550_data_ready ( struct uart *uart ) {
	struct ns16550_uart *ns16550 = uart->priv;
	uint8_t lsr;

	/* Check for receive data ready */
	lsr = ns16550_read ( ns16550, NS16550_LSR );
	return ( lsr & NS16550_LSR_DR );
}

/**
 * Receive data
 *
 * @v uart		UART
 * @ret data		Data
 */
static uint8_t ns16550_receive ( struct uart *uart ) {
	struct ns16550_uart *ns16550 = uart->priv;
	uint8_t rbr;

	/* Receive byte */
	rbr = ns16550_read ( ns16550, NS16550_RBR );
	return rbr;
}

/**
 * Flush transmitted data
 *
 * @v uart		UART
 */
static void ns16550_flush ( struct uart *uart ) {
	struct ns16550_uart *ns16550 = uart->priv;
	unsigned int i;
	uint8_t lsr;

	/* Wait for transmitter to become empty */
	for ( i = 0 ; i < NS16550_TEMT_TIMEOUT_MS ; i++ ) {
		lsr = ns16550_read ( ns16550, NS16550_LSR );
		if ( lsr & NS16550_LSR_TEMT )
			break;
	}
}

/**
 * Initialise UART
 *
 * @v uart		UART
 * @v baud		Baud rate, or zero to leave unchanged
 * @ret rc		Return status code
 */
static int ns16550_init ( struct uart *uart, unsigned int baud ) {
	struct ns16550_uart *ns16550 = uart->priv;
	uint8_t dlm;
	uint8_t dll;

	/* Fail if UART scratch register seems not to be present */
	ns16550_write ( ns16550, NS16550_SCR, 0x18 );
	if ( ns16550_read ( ns16550, NS16550_SCR ) != 0x18 )
		return -ENODEV;
	ns16550_write ( ns16550, NS16550_SCR, 0xae );
	if ( ns16550_read ( ns16550, NS16550_SCR ) != 0xae )
		return -ENODEV;

	/* Wait for UART to become idle before modifying LCR */
	ns16550_flush ( uart );

	/* Configure divisor and line control register, if applicable */
	ns16550_write ( ns16550, NS16550_LCR,
			( NS16550_LCR_8N1 | NS16550_LCR_DLAB ) );
	if ( baud ) {
		ns16550->divisor = ( ( ns16550->clock / baud ) /
				     NS16550_CLK_BIT );
		dlm = ( ( ns16550->divisor >> 8 ) & 0xff );
		dll = ( ( ns16550->divisor >> 0 ) & 0xff );
		ns16550_write ( ns16550, NS16550_DLM, dlm );
		ns16550_write ( ns16550, NS16550_DLL, dll );
	} else {
		dlm = ns16550_read ( ns16550, NS16550_DLM );
		dll = ns16550_read ( ns16550, NS16550_DLL );
		ns16550->divisor = ( ( dlm << 8 ) | dll );
	}
	ns16550_write ( ns16550, NS16550_LCR, NS16550_LCR_8N1 );

	/* Disable interrupts */
	ns16550_write ( ns16550, NS16550_IER, 0 );

	/* Enable FIFOs */
	ns16550_write ( ns16550, NS16550_FCR, NS16550_FCR_FE );

	/* Assert DTR and RTS */
	ns16550_write ( ns16550, NS16550_MCR,
			( NS16550_MCR_DTR | NS16550_MCR_RTS ) );

	/* Flush any stale received data */
	while ( ns16550_data_ready ( uart ) )
		ns16550_receive ( uart );

	return 0;
}

/** 16550 UART operations */
struct uart_operations ns16550_operations = {
	.transmit = ns16550_transmit,
	.data_ready = ns16550_data_ready,
	.receive = ns16550_receive,
	.init = ns16550_init,
	.flush = ns16550_flush,
};
