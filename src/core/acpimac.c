/*
 * Copyright (C) 2021 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <errno.h>
#include <ipxe/acpi.h>
#include <ipxe/base16.h>
#include <ipxe/ethernet.h>
#include <ipxe/if_ether.h>
#include <ipxe/acpimac.h>

/** @file
 *
 * ACPI MAC address
 *
 */

/** Colour for debug messages */
#define colour FADT_SIGNATURE

/** AMAC signature */
#define AMAC_SIGNATURE ACPI_SIGNATURE ( 'A', 'M', 'A', 'C' )

/** MACA signature */
#define MACA_SIGNATURE ACPI_SIGNATURE ( 'M', 'A', 'C', 'A' )

/** Maximum number of bytes to skip after AMAC/MACA signature
 *
 * This is entirely empirical.
 */
#define AUXMAC_MAX_SKIP 8

/**
 * Extract MAC address from DSDT/SSDT
 *
 * @v zsdt		DSDT or SSDT
 * @v len		Length of DSDT/SSDT
 * @v offset		Offset of signature within DSDT/SSDT
 * @v data		Data buffer
 * @ret rc		Return status code
 *
 * Some vendors provide a "system MAC address" within the DSDT/SSDT,
 * to be used to override the MAC address for a USB docking station.
 *
 * A full implementation would require an ACPI bytecode interpreter,
 * since at least one OEM allows the MAC address to be constructed by
 * executable ACPI bytecode (rather than a fixed data structure).
 *
 * We instead attempt to extract a plausible-looking "_AUXMAC_#.....#"
 * string that appears shortly after an "AMAC" or "MACA" signature.
 * This should work for most implementations encountered in practice.
 */
static int acpi_extract_mac ( userptr_t zsdt, size_t len, size_t offset,
			      void *data ) {
	static const char prefix[9] = "_AUXMAC_#";
	uint8_t *hw_addr = data;
	size_t skip = 0;
	char auxmac[ sizeof ( prefix ) /* "_AUXMAC_#" */ +
		     ( ETH_ALEN * 2 ) /* MAC */ + 1 /* "#" */ + 1 /* NUL */ ];
	char *mac = &auxmac[ sizeof ( prefix ) ];
	int decoded_len;
	int rc;

	/* Skip signature and at least one tag byte */
	offset += ( 4 /* signature */ + 1 /* tag byte */ );

	/* Scan for "_AUXMAC_#.....#" close to signature */
	for ( skip = 0 ;
	      ( ( skip < AUXMAC_MAX_SKIP ) &&
		( offset + skip + sizeof ( auxmac ) ) < len ) ;
	      skip++ ) {

		/* Read value */
		copy_from_user ( auxmac, zsdt, ( offset + skip ),
				 sizeof ( auxmac ) );

		/* Check for expected format */
		if ( memcmp ( auxmac, prefix, sizeof ( prefix ) ) != 0 )
			continue;
		if ( auxmac[ sizeof ( auxmac ) - 2 ] != '#' )
			continue;
		if ( auxmac[ sizeof ( auxmac ) - 1 ] != '\0' )
			continue;
		DBGC ( colour, "ACPI found MAC string \"%s\"\n", auxmac );

		/* Terminate MAC address string */
		mac = &auxmac[ sizeof ( prefix ) ];
		mac[ ETH_ALEN * 2 ] = '\0';

		/* Decode MAC address */
		decoded_len = base16_decode ( mac, hw_addr, ETH_ALEN );
		if ( decoded_len < 0 ) {
			rc = decoded_len;
			DBGC ( colour, "ACPI could not decode MAC \"%s\": %s\n",
			       mac, strerror ( rc ) );
			return rc;
		}

		/* Check MAC address validity */
		if ( ! is_valid_ether_addr ( hw_addr ) ) {
			DBGC ( colour, "ACPI has invalid MAC %s\n",
			       eth_ntoa ( hw_addr ) );
			return -EINVAL;
		}

		return 0;
	}

	return -ENOENT;
}

/**
 * Extract MAC address from DSDT/SSDT
 *
 * @v hw_addr		MAC address to fill in
 * @ret rc		Return status code
 */
int acpi_mac ( uint8_t *hw_addr ) {
	int rc;

	/* Look for an "AMAC" address */
	if ( ( rc = acpi_extract ( AMAC_SIGNATURE, hw_addr,
				   acpi_extract_mac ) ) == 0 )
		return 0;

	/* Look for a "MACA" address */
	if ( ( rc = acpi_extract ( MACA_SIGNATURE, hw_addr,
				   acpi_extract_mac ) ) == 0 )
		return 0;

	return -ENOENT;
}
