/*
 * Copyright (C) 2026 Bejean Mosher <bejean.mosher@intel.com>.
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
 * Universal Payload serial console
 *
 * The serial console is described by the first "serial@" device tree
 * node, with the address space identified as for EDK2's FdtParserLib.
 *
 */

#include <stdint.h>
#include <ipxe/io.h>
#include <ipxe/serial.h>
#include <ipxe/ns16550.h>
#include <ipxe/fdt.h>
#include <ipxe/upl.h>

#ifdef SERIAL_UPL
#define SERIAL_PREFIX_upl
#else
#define SERIAL_PREFIX_upl __upl_
#endif

/** Maximum supported register shift */
#define UPL_SERIAL_MAX_SHIFT 3

/** PCI address space code mask within first address cell */
#define UPL_SERIAL_PCI_SPACE_MASK 0x03000000

/** PCI I/O space code within first address cell */
#define UPL_SERIAL_PCI_SPACE_IO 0x01000000

/** Universal Payload 16550 UART */
static struct ns16550_uart upl_ns16550 = {
	.clock = NS16550_CLK_DEFAULT,
};

/** Universal Payload UART */
static struct uart upl_uart = {
	.refcnt = REF_INIT ( ref_no_free ),
	.name = "UPL",
	.op = &ns16550_operations,
	.priv = &upl_ns16550,
};

/**
 * Identify default serial console
 *
 * @ret uart		Default serial console UART, or NULL
 */
static struct uart * upl_serial_console ( void ) {
	struct ns16550_uart *ns16550 = &upl_ns16550;
	struct uart *uart = &upl_uart;
	struct fdt_reg_cells regs;
	unsigned int offset;
	uint64_t space;
	uint64_t base;
	uint32_t shift;
	int mmio;

	/* Locate serial port */
	if ( ( upl_fdt_find ( "serial@", NULL, &offset ) != 0 ) ||
	     ( fdt_parent_reg_cells ( &sysfdt, offset, &regs ) != 0 ) ) {
		DBGC ( uart, "UPL found no serial port\n" );
		goto fallback;
	}

	/* Identify address space */
	switch ( regs.address_cells ) {
	case 1:
		if ( fdt_cells ( &sysfdt, offset, "reg", 0, 1, &base ) != 0 )
			goto malformed;
		mmio = ( base >= 0x10000 );
		break;
	case 2:
		if ( ( fdt_cells ( &sysfdt, offset, "reg", 0, 1,
				   &space ) != 0 ) ||
		     ( fdt_cells ( &sysfdt, offset, "reg", 1, 1,
				   &base ) != 0 ) )
			goto malformed;
		mmio = ( space != 1 );
		if ( mmio )
			base |= ( space << 32 );
		break;
	case 3:
		if ( ( fdt_cells ( &sysfdt, offset, "reg", 0, 1,
				   &space ) != 0 ) ||
		     ( fdt_cells ( &sysfdt, offset, "reg", 1, 2,
				   &base ) != 0 ) )
			goto malformed;
		mmio = ( ( space & UPL_SERIAL_PCI_SPACE_MASK ) !=
			 UPL_SERIAL_PCI_SPACE_IO );
		break;
	default:
		goto malformed;
	}

	/* Get register shift, if present */
	if ( fdt_u32 ( &sysfdt, offset, "reg-shift", &shift ) != 0 )
		shift = 0;
	DBGC ( uart, "UPL serial port %s %#08llx shift %d\n",
	       ( mmio ? "MMIO" : "I/O" ), ( ( unsigned long long ) base ),
	       shift );
	if ( shift > UPL_SERIAL_MAX_SHIFT ) {
		DBGC ( uart, "UPL unsupported serial shift %d\n", shift );
		goto fallback;
	}
	ns16550->shift = shift;

	/* Map registers */
	if ( mmio ) {
		if ( base > ~( ( physaddr_t ) 0 ) ) {
			DBGC ( uart, "UPL serial port is inaccessible\n" );
			goto fallback;
		}
		ns16550->base = ioremap ( base, ( NS16550_LEN << shift ) );
		if ( ! ns16550->base ) {
			DBGC ( uart, "UPL could not map serial port\n" );
			goto fallback;
		}
	} else {
		if ( base >= 0x10000 ) {
			DBGC ( uart, "UPL serial port is not an I/O port\n" );
			goto fallback;
		}
		ns16550->base = ( ( void * ) ( intptr_t ) base );
	}

	/* Preserve the divisor already programmed by the bootloader,
	 * since the UART input clock frequency is unknown.
	 */
	uart->baud = 0;

	return uart;

 malformed:
	DBGC ( uart, "UPL serial port has unsupported address\n" );
 fallback:
	return fixed_serial_console();
}

PROVIDE_SERIAL ( upl, default_serial_console, upl_serial_console );
