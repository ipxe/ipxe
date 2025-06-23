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
 * DesignWare UART
 *
 */

#include <errno.h>
#include <ipxe/uart.h>
#include <ipxe/ns16550.h>
#include <ipxe/devtree.h>
#include <ipxe/fdt.h>

/**
 * Probe devicetree device
 *
 * @v dt		Devicetree device
 * @v offset		Starting node offset
 * @ret rc		Return status code
 */
static int dwuart_probe ( struct dt_device *dt, unsigned int offset ) {
	struct ns16550_uart *ns16550;
	struct uart *uart;
	uint32_t shift;
	uint32_t clock;
	int rc;

	/* Allocate and initialise UART */
	uart = alloc_uart ( sizeof ( *ns16550 ) );
	if ( ! uart ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	uart->name = dt->name;
	uart->op = &ns16550_operations;
	ns16550 = uart->priv;
	dt_set_drvdata ( dt, uart );

	/* Map registers */
	ns16550->base = dt_ioremap ( dt, offset, 0, 0 );
	if ( ! ns16550->base ) {
		rc = -ENODEV;
		goto err_ioremap;
	}

	/* Get register shift */
	if ( ( rc = fdt_u32 ( &sysfdt, offset, "reg-shift", &shift ) ) != 0 )
		shift = 0;
	ns16550->shift = shift;

	/* Get clock rate */
	if ( ( rc = fdt_u32 ( &sysfdt, offset, "clock-frequency",
			      &clock ) ) != 0 ) {
		clock = NS16550_CLK_DEFAULT;
	}
	ns16550->clock = clock;

	/* Register UART */
	if ( ( rc = uart_register ( uart ) ) != 0 )
		goto err_register;

	return 0;

	uart_unregister ( uart );
 err_register:
	iounmap ( ns16550->base );
 err_ioremap:
	uart_nullify ( uart );
	uart_put ( uart );
 err_alloc:
	return rc;
}

/**
 * Remove devicetree device
 *
 * @v dt		Devicetree device
 */
static void dwuart_remove ( struct dt_device *dt ) {
	struct uart *uart = dt_get_drvdata ( dt );
	struct ns16550_uart *ns16550 = uart->priv;

	/* Unregister UART */
	uart_unregister ( uart );

	/* Free UART */
	iounmap ( ns16550->base );
	uart_nullify ( uart );
	uart_put ( uart );
}

/** DesignWare UART compatible model identifiers */
static const char * dwuart_ids[] = {
	"snps,dw-apb-uart",
	"ns16550a",
};

/** DesignWare UART devicetree driver */
struct dt_driver dwuart_driver __dt_driver = {
	.name = "dwuart",
	.ids = dwuart_ids,
	.id_count = ( sizeof ( dwuart_ids ) / sizeof ( dwuart_ids[0] ) ),
	.probe = dwuart_probe,
	.remove = dwuart_remove,
};
