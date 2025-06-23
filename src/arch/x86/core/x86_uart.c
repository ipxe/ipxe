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

#include <string.h>
#include <ipxe/serial.h>
#include <ipxe/ns16550.h>

/** Define a fixed ISA UART */
#define ISA_UART( NAME, BASE )						\
	static struct ns16550_uart ns16550_ ## NAME = {			\
		.base = ( ( void * ) (BASE) ),				\
		.clock = NS16550_CLK_DEFAULT,				\
	};								\
	struct uart NAME = {						\
		.refcnt = REF_INIT ( ref_no_free ),			\
		.name = #NAME,						\
		.op = &ns16550_operations,				\
		.priv = &ns16550_ ## NAME,				\
	}

/* Fixed ISA UARTs */
ISA_UART ( com1, COM1_BASE );
ISA_UART ( com2, COM2_BASE );
ISA_UART ( com3, COM3_BASE );
ISA_UART ( com4, COM4_BASE );

/**
 * Register fixed ISA UARTs
 *
 * @ret rc		Return status code
 */
int uart_register_fixed ( void ) {
	static struct uart *ports[] = { COM1, COM2, COM3, COM4 };
	unsigned int i;
	int rc;

	/* Register all fixed ISA UARTs */
	for ( i = 0 ; i < ( sizeof ( ports ) / sizeof ( ports[0] ) ) ; i++ ) {
		if ( ( rc = uart_register ( ports[i] ) ) != 0 ) {
			DBGC ( ports[i], "UART could not register %s: %s\n",
			       ports[i]->name, strerror ( rc ) );
			return rc;
		}
	}

	return 0;
}
