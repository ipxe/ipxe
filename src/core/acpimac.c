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
#include <ipxe/settings.h>
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

/** RTMA signature */
#define RTMA_SIGNATURE ACPI_SIGNATURE ( 'R', 'T', 'M', 'A' )

/** Maximum number of bytes to skip after ACPI signature
 *
 * This is entirely empirical.
 */
#define ACPIMAC_MAX_SKIP 8

/** An ACPI MAC extraction mechanism */
struct acpimac_extractor {
	/** Prefix string */
	const char *prefix;
	/** Encoded MAC length */
	size_t len;
	/** Decode MAC
	 *
	 * @v mac	Encoded MAC
	 * @v hw_addr	MAC address to fill in
	 * @ret rc	Return status code
	 */
	int ( * decode ) ( const char *mac, uint8_t *hw_addr );
};

/**
 * Decode Base16-encoded MAC address
 *
 * @v mac		Encoded MAC
 * @v hw_addr		MAC address to fill in
 * @ret rc		Return status code
 */
static int acpimac_decode_base16 ( const char *mac, uint8_t *hw_addr ) {
	int len;
	int rc;

	/* Attempt to base16-decode MAC address */
	len = base16_decode ( mac, hw_addr, ETH_ALEN );
	if ( len < 0 ) {
		rc = len;
		DBGC ( colour, "ACPI could not decode base16 MAC \"%s\": %s\n",
		       mac, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Decode raw MAC address
 *
 * @v mac		Encoded MAC
 * @v hw_addr		MAC address to fill in
 * @ret rc		Return status code
 */
static int acpimac_decode_raw ( const char *mac, uint8_t *hw_addr ) {

	memcpy ( hw_addr, mac, ETH_ALEN );
	return 0;
}

/** "_AUXMAC_" extraction mechanism */
static struct acpimac_extractor acpimac_auxmac = {
	.prefix = "_AUXMAC_#",
	.len = ( ETH_ALEN * 2 ),
	.decode = acpimac_decode_base16,
};

/** "_RTXMAC_" extraction mechanism */
static struct acpimac_extractor acpimac_rtxmac = {
	.prefix = "_RTXMAC_#",
	.len = ETH_ALEN,
	.decode = acpimac_decode_raw,
};

/**
 * Extract MAC address from DSDT/SSDT
 *
 * @v zsdt		DSDT or SSDT
 * @v len		Length of DSDT/SSDT
 * @v offset		Offset of signature within DSDT/SSDT
 * @v data		Data buffer
 * @v extractor		ACPI MAC address extractor
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
static int acpimac_extract ( const struct acpi_header *zsdt, size_t len,
			     size_t offset, void *data,
			     struct acpimac_extractor *extractor ) {
	size_t prefix_len = strlen ( extractor->prefix );
	uint8_t *hw_addr = data;
	size_t skip = 0;
	char buf[ prefix_len + extractor->len + 1 /* "#" */ + 1 /* NUL */ ];
	char *mac = &buf[prefix_len];
	int rc;

	/* Skip signature and at least one tag byte */
	offset += ( 4 /* signature */ + 1 /* tag byte */ );

	/* Scan for suitable string close to signature */
	for ( skip = 0 ;
	      ( ( skip < ACPIMAC_MAX_SKIP ) &&
		( offset + skip + sizeof ( buf ) ) <= len ) ;
	      skip++ ) {

		/* Read value */
		memcpy ( buf, ( ( ( const void * ) zsdt ) + offset + skip ),
			 sizeof ( buf ) );

		/* Check for expected format */
		if ( memcmp ( buf, extractor->prefix, prefix_len ) != 0 )
			continue;
		if ( buf[ sizeof ( buf ) - 2 ] != '#' )
			continue;
		if ( buf[ sizeof ( buf ) - 1 ] != '\0' )
			continue;
		DBGC ( colour, "ACPI found MAC:\n" );
		DBGC_HDA ( colour, ( offset + skip ), buf, sizeof ( buf ) );

		/* Terminate MAC address string */
		mac[extractor->len] = '\0';

		/* Decode MAC address */
		if ( ( rc = extractor->decode ( mac, hw_addr ) ) != 0 )
			return rc;

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
 * Extract "_AUXMAC_" MAC address from DSDT/SSDT
 *
 * @v zsdt		DSDT or SSDT
 * @v len		Length of DSDT/SSDT
 * @v offset		Offset of signature within DSDT/SSDT
 * @v data		Data buffer
 * @ret rc		Return status code
 */
static int acpimac_extract_auxmac ( const struct acpi_header *zsdt,
				    size_t len, size_t offset, void *data ) {

	return acpimac_extract ( zsdt, len, offset, data, &acpimac_auxmac );
}

/**
 * Extract "_RTXMAC_" MAC address from DSDT/SSDT
 *
 * @v zsdt		DSDT or SSDT
 * @v len		Length of DSDT/SSDT
 * @v offset		Offset of signature within DSDT/SSDT
 * @v data		Data buffer
 * @ret rc		Return status code
 */
static int acpimac_extract_rtxmac ( const struct acpi_header *zsdt,
				    size_t len, size_t offset, void *data ) {

	return acpimac_extract ( zsdt, len, offset, data, &acpimac_rtxmac );
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
				   acpimac_extract_auxmac ) ) == 0 )
		return 0;

	/* Look for a "MACA" address */
	if ( ( rc = acpi_extract ( MACA_SIGNATURE, hw_addr,
				   acpimac_extract_auxmac ) ) == 0 )
		return 0;

	/* Look for a "RTMA" address */
	if ( ( rc = acpi_extract ( RTMA_SIGNATURE, hw_addr,
				   acpimac_extract_rtxmac ) ) == 0 )
		return 0;

	return -ENOENT;
}

/**
 * Fetch system MAC address setting
 *
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int sysmac_fetch ( void *data, size_t len ) {
	uint8_t mac[ETH_ALEN];
	int rc;

	/* Try fetching ACPI MAC address */
	if ( ( rc = acpi_mac ( mac ) ) != 0 )
		return rc;

	/* Return MAC address */
	if ( len > sizeof ( mac ) )
		len = sizeof ( mac );
	memcpy ( data, mac, len );
	return ( sizeof ( mac ) );
}

/** System MAC address setting */
const struct setting sysmac_setting __setting ( SETTING_MISC, sysmac ) = {
	.name = "sysmac",
	.description = "System MAC",
	.type = &setting_type_hex,
	.scope = &builtin_scope,
};

/** System MAC address built-in setting */
struct builtin_setting sysmac_builtin_setting __builtin_setting = {
	.setting = &sysmac_setting,
	.fetch = sysmac_fetch,
};
