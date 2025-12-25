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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <ipxe/serial.h>
#include <ipxe/settings.h>
#include <ipxe/pci.h>
#include <ipxe/ns16550.h>
#include <ipxe/spcr.h>
#if defined ( __i386__ ) || defined ( __x86_64__ )
#include <ipxe/x86_io.h>
#endif

/** @file
 *
 * ACPI Serial Port Console Redirection (SPCR)
 *
 */

#ifdef SERIAL_SPCR
#define SERIAL_PREFIX_spcr
#else
#define SERIAL_PREFIX_spcr __spcr_
#endif

/** SPCR-defined UART */
static struct uart spcr_uart = {
	.refcnt = REF_INIT ( ref_no_free ),
	.name = "SPCR",
};

/** SPCR-defined 16550 UART */
static struct ns16550_uart spcr_ns16550 = {
	.clock = NS16550_CLK_DEFAULT,
};

/** Base baud rate for SPCR divisors */
#define SPCR_BAUD_BASE 115200

/** SPCR baud rate divisors */
static const uint8_t spcr_baud_divisor[SPCR_BAUD_MAX] = {
	[SPCR_BAUD_2400] = ( SPCR_BAUD_BASE / 2400 ),
	[SPCR_BAUD_4800] = ( SPCR_BAUD_BASE / 4800 ),
	[SPCR_BAUD_9600] = ( SPCR_BAUD_BASE / 9600 ),
	[SPCR_BAUD_19200] = ( SPCR_BAUD_BASE / 19200 ),
	[SPCR_BAUD_38400] = ( SPCR_BAUD_BASE / 38400 ),
	[SPCR_BAUD_57600] = ( SPCR_BAUD_BASE / 57600 ),
	[SPCR_BAUD_115200] = ( SPCR_BAUD_BASE / 115200 ),
};

/**
* Fetch SPCR setting.
*
* @v data             Buffer to fill with setting data
* @v len              Length of buffer
* @ret len            Length of setting data, or negative error
*/
static int spcr_fetch ( void *data, size_t len ) {
	if ( ! spcr_uart.priv )
		return -ENOENT;

	int written;
#if defined ( __i386__ ) || defined ( __x86_64__ )
	const char *iotype = x86_pio_addr( ( intptr_t ) spcr_ns16550.base ) ? "io" : "mmio";
#else
	const char *iotype = "mmio";
#endif
	char buf[32];

	if ( ! spcr_uart.baud )
		written = snprintf( buf, sizeof( buf ), "uart,%s,%p", iotype, spcr_ns16550.base  );
	else
		written = snprintf( buf, sizeof( buf ), "uart,%s,%p,%dn8", iotype, spcr_ns16550.base , ( int ) spcr_uart.baud );

	if ( written < 0 )
		return -EINVAL;

	if ( ( size_t ) written > len )
		return written;

	memcpy(data, buf, written);
	return written;
}

/** SPCR setting */
const struct setting spcr_setting __setting ( SETTING_MISC, spcr ) = {
	.name = "spcr",
	.description = "Linux compatible SPCR console configuration",
	.type = &setting_type_string,
	.scope = &builtin_scope,
};

/** SPCR built-in setting */
struct builtin_setting spcr_builtin_setting __builtin_setting = {
	.setting = &spcr_setting,
	.fetch = spcr_fetch
};

/**
 * Configure 16550-based serial console
 *
 * @v spcr		SPCR table
 * @v uart		UART to configure
 * @ret rc		Return status code
 */
static int spcr_16550 ( struct spcr_table *spcr, struct uart *uart ) {
	struct ns16550_uart *ns16550 = &spcr_ns16550;

	/* Set base address */
	ns16550->base = acpi_ioremap ( &spcr->base, NS16550_LEN );
	if ( ! ns16550->base ) {
		DBGC ( uart, "SPCR could not map registers\n" );
		return -ENODEV;
	}

	/* Set clock frequency, if specified */
	if ( spcr->clock )
		ns16550->clock = le32_to_cpu ( spcr->clock );

	/* Configure UART as a 16550 */
	uart->op = &ns16550_operations;
	uart->priv = ns16550;

	return 0;
}

/**
 * Identify default serial console
 *
 * @ret uart		Default serial console UART, or NULL
 */
static struct uart * spcr_console ( void ) {
	struct uart *uart = &spcr_uart;
	struct spcr_table *spcr;
	unsigned int baud;
	int rc;

	/* Locate SPCR table */
	spcr = container_of ( acpi_table ( SPCR_SIGNATURE, 0 ),
			      struct spcr_table, acpi );
	if ( ! spcr ) {
		DBGC ( uart, "SPCR found no table\n" );
		goto err_table;
	}
	DBGC2 ( uart, "SPCR found table:\n" );
	DBGC2_HDA ( uart, 0, spcr, sizeof ( *spcr ) );
	DBGC ( uart, "SPCR is type %d at %02x:%08llx\n",
	       spcr->type, spcr->base.type,
	       ( ( unsigned long long ) le64_to_cpu ( spcr->base.address ) ) );
	if ( spcr->pci_vendor_id != cpu_to_le16 ( PCI_ANY_ID ) ) {
		DBGC ( uart, "SPCR is PCI " PCI_FMT " (%04x:%04x)\n",
		       spcr->pci_segment, spcr->pci_bus, spcr->pci_dev,
		       spcr->pci_func, le16_to_cpu ( spcr->pci_vendor_id ),
		       le16_to_cpu ( spcr->pci_device_id ) );
	}

	/* Get baud rate */
	baud = 0;
	if ( le32_to_cpu ( spcr->acpi.length ) >=
	     ( offsetof ( typeof ( *spcr ), precise ) +
	       sizeof ( spcr->precise ) ) ) {
		baud = le32_to_cpu ( spcr->precise );
		if ( baud )
			DBGC ( uart, "SPCR has precise baud rate %d\n", baud );
	}
	if ( ( ! baud ) && spcr->baud && ( spcr->baud < SPCR_BAUD_MAX ) ) {
		baud = ( SPCR_BAUD_BASE / spcr_baud_divisor[spcr->baud] );
		DBGC ( uart, "SPCR has baud rate %d\n", baud );
	}
	uart->baud = baud;

	/* Initialise according to type */
	switch ( spcr->type ) {
	case SPCR_TYPE_16550:
	case SPCR_TYPE_16450:
	case SPCR_TYPE_16550_GAS:
		if ( ( rc = spcr_16550 ( spcr, uart ) ) != 0 )
			goto err_type;
		break;
	default:
		DBGC ( uart, "SPCR unsupported type %d\n", spcr->type );
		goto err_type;
	}

	return uart;

 err_type:
 err_table:
	/* Fall back to using fixed serial console */
	return fixed_serial_console();
}

PROVIDE_SERIAL ( spcr, default_serial_console, spcr_console );
