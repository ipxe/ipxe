/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <string.h>
#include <ipxe/serial.h>
#include <ipxe/devtree.h>
#include <ipxe/fdt.h>

/** @file
 *
 * Flattened Device Tree serial console
 *
 */

#ifdef SERIAL_FDT
#define SERIAL_PREFIX_fdt
#else
#define SERIAL_PREFIX_fdt __fdt_
#endif

/** FDT console parent device */
static struct device fdtcon_parent = {
	.name = "fdtcon",
	.siblings = LIST_HEAD_INIT ( fdtcon_parent.siblings ),
	.children = LIST_HEAD_INIT ( fdtcon_parent.children ),
};

/** Colour for debug messages */
#define colour &fdtcon_parent

/**
 * Identify default serial console
 *
 * @ret uart		Default serial console UART, or NULL
 */
static struct uart * fdtcon_default ( void ) {
	unsigned int chosen;
	unsigned int stdout;
	const char *path;
	struct uart *prev;
	struct uart *uart;
	int rc;

	/* Record existing UART, if any */
	prev = list_last_entry ( &uarts, struct uart, list );

	/* Locate "/chosen" node */
	if ( ( rc = fdt_path ( &sysfdt, "/chosen", &chosen ) ) != 0 ) {
		DBGC ( colour, "FDTCON could not locate \"/chosen\": %s\n",
		       strerror ( rc ) );
		return NULL;
	}

	/* Get console device path (or alias) */
	path = fdt_string ( &sysfdt, chosen, "stdout-path" );
	if ( ! path ) {
		DBGC ( colour, "FDTCON has no console device\n" );
		return NULL;
	}
	DBGC ( colour, "FDTCON console device is \"%s\"\n", path );

	/* Locate device */
	if ( ( ( rc = fdt_path ( &sysfdt, path, &stdout ) ) != 0 ) &&
	     ( ( rc = fdt_alias ( &sysfdt, path, &stdout ) ) != 0 ) ) {
		DBGC ( colour, "FDTCON could not locate \"/%s\": %s\n",
		       path, strerror ( rc ) );
		return NULL;
	}

	/* Probe device */
	if ( ( rc = dt_probe_node ( &fdtcon_parent, stdout ) ) != 0 ) {
		DBGC ( colour, "FDTCON could not probe \"%s\": %s\n",
		       path, strerror ( rc ) );
		return NULL;
	}

	/* Use newly added UART, if any */
	uart = list_last_entry ( &uarts, struct uart, list );
	if ( uart == prev )
		return NULL;

	DBGC ( colour, "FDTCON using UART %s\n", uart->name );
	return uart;
}

PROVIDE_SERIAL ( fdt, default_serial_console, fdtcon_default );
