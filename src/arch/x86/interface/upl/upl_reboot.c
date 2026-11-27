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
 * Universal Payload reboot mechanism
 *
 */

#include <string.h>
#include <unistd.h>
#include <byteswap.h>
#include <ipxe/io.h>
#include <ipxe/acpi.h>
#include <ipxe/reboot.h>
#include <ipxe/acpipwr.h>

#ifdef REBOOT_UPL
#define REBOOT_PREFIX_upl
#else
#define REBOOT_PREFIX_upl __upl_
#endif

/** Offset of flags within FADT */
#define FADT_FLAGS 112

/** FADT reset register supported flag */
#define FADT_FLAGS_RESET_REG_SUP 0x00000400UL

/** Offset of reset register within FADT */
#define FADT_RESET_REG 116

/** Offset of reset value within FADT */
#define FADT_RESET_VALUE 128

/** Reset control register */
#define UPL_RST_CNT 0xcf9
#define UPL_RST_CNT_SYS_RST	0x02	/**< System reset */
#define UPL_RST_CNT_RST_CPU	0x04	/**< Reset CPU */
#define UPL_RST_CNT_FULL_RST	0x08	/**< Full (cold) reset */

/** Keyboard controller command port */
#define UPL_KBC_CMD 0x64

/** Keyboard controller pulse reset line command */
#define UPL_KBC_RESET 0xfe

/**
 * Reset via ACPI reset register
 *
 */
static void upl_reboot_acpi ( void ) {
	const struct acpi_header *fadt;
	struct acpi_address reset_reg;
	uint32_t flags;
	uint8_t value;
	void *reg;

	/* Locate FADT and check that reset register is supported */
	fadt = acpi_table ( FADT_SIGNATURE, 0 );
	if ( ! fadt )
		return;
	if ( le32_to_cpu ( fadt->length ) <= FADT_RESET_VALUE )
		return;
	memcpy ( &flags, ( ( ( const void * ) fadt ) + FADT_FLAGS ),
		 sizeof ( flags ) );
	if ( ! ( le32_to_cpu ( flags ) & FADT_FLAGS_RESET_REG_SUP ) )
		return;
	memcpy ( &reset_reg, ( ( ( const void * ) fadt ) + FADT_RESET_REG ),
		 sizeof ( reset_reg ) );
	value = *( ( ( const uint8_t * ) fadt ) + FADT_RESET_VALUE );

	/* Write reset value */
	reg = acpi_ioremap ( &reset_reg, sizeof ( value ) );
	if ( ! reg )
		return;
	DBGC ( fadt, "UPL resetting via ACPI register %02x:%08llx\n",
	       reset_reg.type,
	       ( ( unsigned long long ) le64_to_cpu ( reset_reg.address ) ) );
	iowrite8 ( value, reg );
	mdelay ( 100 );
}

/**
 * Reboot system
 *
 * @v flags		Reboot flags
 */
static void upl_reboot ( int flags ) {
	uint8_t type;

	/* Try ACPI reset register (for warm reboots) */
	if ( flags & REBOOT_WARM )
		upl_reboot_acpi();

	/* Try reset control register */
	type = ( UPL_RST_CNT_SYS_RST | UPL_RST_CNT_RST_CPU );
	if ( ! ( flags & REBOOT_WARM ) )
		type |= UPL_RST_CNT_FULL_RST;
	outb ( UPL_RST_CNT_SYS_RST, UPL_RST_CNT );
	outb ( type, UPL_RST_CNT );
	mdelay ( 100 );

	/* Try ACPI reset register (for cold reboots) */
	upl_reboot_acpi();

	/* Try keyboard controller */
	outb ( UPL_KBC_RESET, UPL_KBC_CMD );
	mdelay ( 100 );
}

/**
 * Power off system
 *
 * @ret rc		Return status code
 */
static int upl_poweroff ( void ) {

	return acpi_poweroff();
}

PROVIDE_REBOOT ( upl, reboot, upl_reboot );
PROVIDE_REBOOT ( upl, poweroff, upl_poweroff );
