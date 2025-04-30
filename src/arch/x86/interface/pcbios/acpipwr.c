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

#include <string.h>
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
 * Extract \_Sx value from DSDT/SSDT
 *
 * @v zsdt		DSDT or SSDT
 * @v len		Length of DSDT/SSDT
 * @v offset		Offset of signature within DSDT/SSDT
 * @v data		Data buffer
 * @ret rc		Return status code
 *
 * In theory, extracting the \_Sx value from the DSDT/SSDT requires a
 * full ACPI parser plus some heuristics to work around the various
 * broken encodings encountered in real ACPI implementations.
 *
 * In practice, we can get the same result by scanning through the
 * DSDT/SSDT for the signature (e.g. "_S5_"), extracting the first
 * four bytes, removing any bytes with bit 3 set, and treating
 * whatever is left as a little-endian value.  This is one of the
 * uglier hacks I have ever implemented, but it's still prettier than
 * the ACPI specification itself.
 */
static int acpi_extract_sx ( const struct acpi_header *zsdt, size_t len,
			     size_t offset, void *data ) {
	unsigned int *sx = data;
	uint8_t bytes[4];
	uint8_t *byte;

	/* Skip signature and package header */
	offset += ( 4 /* signature */ + 3 /* package header */ );

	/* Sanity check */
	if ( ( offset + sizeof ( bytes ) /* value */ ) > len ) {
		return -EINVAL;
	}

	/* Read first four bytes of value */
	memcpy ( bytes, ( ( ( const void * ) zsdt ) + offset ),
		 sizeof ( bytes ) );
	DBGC ( colour, "ACPI found \\_Sx containing %02x:%02x:%02x:%02x\n",
	       bytes[0], bytes[1], bytes[2], bytes[3] );

	/* Extract \Sx value.  There are three potential encodings
	 * that we might encounter:
	 *
	 * - SLP_TYPa, SLP_TYPb, rsvd, rsvd
	 *
	 * - <byteprefix>, SLP_TYPa, <byteprefix>, SLP_TYPb, ...
	 *
	 * - <dwordprefix>, SLP_TYPa, SLP_TYPb, 0, 0
	 *
	 * Since <byteprefix> and <dwordprefix> both have bit 3 set,
	 * and valid SLP_TYPx must have bit 3 clear (since SLP_TYPx is
	 * a 3-bit field), we can just skip any bytes with bit 3 set.
	 */
	byte = bytes;
	if ( *byte & 0x08 )
		byte++;
	*sx = *(byte++);
	if ( *byte & 0x08 )
		byte++;
	*sx |= ( *byte << 8 );

	return 0;
}

/**
 * Power off the computer using ACPI
 *
 * @ret rc		Return status code
 */
int acpi_poweroff ( void ) {
	const struct acpi_fadt *fadt;
	unsigned int pm1a_cnt_blk;
	unsigned int pm1b_cnt_blk;
	unsigned int pm1a_cnt;
	unsigned int pm1b_cnt;
	unsigned int slp_typa;
	unsigned int slp_typb;
	unsigned int s5;
	int rc;

	/* Locate FADT */
	fadt = container_of ( acpi_table ( FADT_SIGNATURE, 0 ),
			      struct acpi_fadt, acpi );
	if ( ! fadt ) {
		DBGC ( colour, "ACPI could not find FADT\n" );
		return -ENOENT;
	}

	/* Read FADT */
	pm1a_cnt_blk = le32_to_cpu ( fadt->pm1a_cnt_blk );
	pm1b_cnt_blk = le32_to_cpu ( fadt->pm1b_cnt_blk );
	pm1a_cnt = ( pm1a_cnt_blk + ACPI_PM1_CNT );
	pm1b_cnt = ( pm1b_cnt_blk + ACPI_PM1_CNT );

	/* Extract \_S5 from DSDT or any SSDT */
	if ( ( rc = acpi_extract ( S5_SIGNATURE, &s5,
				   acpi_extract_sx ) ) != 0 ) {
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
