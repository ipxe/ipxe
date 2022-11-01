/*
 * Copyright (C) 2016 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <unistd.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/io.h>
#include <ipxe/acpi.h>
#include <ipxe/acpipwr.h>

/** @file
 *
 * ACPI power off
 *
 */

/** Colour for debug messages */
#define colour FADT_SIGNATURE

/** _S5_ signature */
#define S5_SIGNATURE ACPI_SIGNATURE ( '_', 'S', '5', '_' )

/**
 * Power off the computer using ACPI
 *
 * @ret rc		Return status code
 */
int acpi_poweroff ( void ) {
	struct acpi_fadt fadtab;
	userptr_t fadt;
	unsigned int pm1a_cnt_blk;
	unsigned int pm1b_cnt_blk;
	unsigned int pm1a_cnt;
	unsigned int pm1b_cnt;
	unsigned int slp_typa;
	unsigned int slp_typb;
	int s5;
	int rc;

	/* Locate FADT */
	fadt = acpi_find ( FADT_SIGNATURE, 0 );
	if ( ! fadt ) {
		DBGC ( colour, "ACPI could not find FADT\n" );
		return -ENOENT;
	}

	/* Read FADT */
	copy_from_user ( &fadtab, fadt, 0, sizeof ( fadtab ) );
	pm1a_cnt_blk = le32_to_cpu ( fadtab.pm1a_cnt_blk );
	pm1b_cnt_blk = le32_to_cpu ( fadtab.pm1b_cnt_blk );
	pm1a_cnt = ( pm1a_cnt_blk + ACPI_PM1_CNT );
	pm1b_cnt = ( pm1b_cnt_blk + ACPI_PM1_CNT );

	/* Extract \_S5 from DSDT or any SSDT */
	s5 = acpi_sx ( S5_SIGNATURE );
	if ( s5 < 0 ) {
		rc = s5;
		DBGC ( colour, "ACPI could not extract \\_S5: %s\n",
		       strerror ( rc ) );
		return rc;
	}

	/* Power off system */
	if ( pm1a_cnt_blk ) {
		slp_typa = ( ( s5 >> 0 ) & 0xff );
		DBGC ( colour, "ACPI PM1a sleep type %#x => %04x\n",
		       slp_typa, pm1a_cnt );
		outw ( ( ACPI_PM1_CNT_SLP_TYP ( slp_typa ) |
			 ACPI_PM1_CNT_SLP_EN ), pm1a_cnt );
	}
	if ( pm1b_cnt_blk ) {
		slp_typb = ( ( s5 >> 8 ) & 0xff );
		DBGC ( colour, "ACPI PM1b sleep type %#x => %04x\n",
		       slp_typb, pm1b_cnt );
		outw ( ( ACPI_PM1_CNT_SLP_TYP ( slp_typb ) |
			 ACPI_PM1_CNT_SLP_EN ), pm1b_cnt );
	}

	/* On some systems, execution will continue briefly.  Delay to
	 * avoid potentially confusing log messages.
	 */
	mdelay ( 1000 );

	DBGC ( colour, "ACPI power off failed\n" );
	return -EPROTO;
}
