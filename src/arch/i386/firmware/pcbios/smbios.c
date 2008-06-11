/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <gpxe/uaccess.h>
#include <realmode.h>
#include <pnpbios.h>
#include <smbios.h>

/** @file
 *
 * System Management BIOS
 *
 */

/** Signature for SMBIOS entry point */
#define SMBIOS_SIGNATURE \
        ( ( '_' << 0 ) + ( 'S' << 8 ) + ( 'M' << 16 ) + ( '_' << 24 ) )

/**
 * SMBIOS entry point
 *
 * This is the single table which describes the list of SMBIOS
 * structures.  It is located by scanning through the BIOS segment.
 */
struct smbios_entry {
	/** Signature
	 *
	 * Must be equal to SMBIOS_SIGNATURE
	 */
	uint32_t signature;
	/** Checksum */
	uint8_t checksum;
	/** Length */
	uint8_t len;
	/** Major version */
	uint8_t major;
	/** Minor version */
	uint8_t minor;
	/** Maximum structure size */
	uint16_t max;
	/** Entry point revision */
	uint8_t revision;
	/** Formatted area */
	uint8_t formatted[5];
	/** DMI Signature */
	uint8_t dmi_signature[5];
	/** DMI checksum */
	uint8_t dmi_checksum;
	/** Structure table length */
	uint16_t smbios_len;
	/** Structure table address */
	physaddr_t smbios_address;
	/** Number of SMBIOS structures */
	uint16_t smbios_count;
	/** BCD revision */
	uint8_t bcd_revision;
} __attribute__ (( packed ));

/**
 * SMBIOS entry point descriptor
 *
 * This contains the information from the SMBIOS entry point that we
 * care about.
 */
struct smbios {
	/** Start of SMBIOS structures */
	userptr_t address;
	/** Length of SMBIOS structures */ 
	size_t len;
	/** Number of SMBIOS structures */
	unsigned int count;
};

/** SMBIOS entry point descriptor */
static struct smbios smbios = {
	.address = UNULL,
};

/**
 * Find SMBIOS
 *
 * @ret rc		Return status code
 */
static int find_smbios ( void ) {
	union {
		struct smbios_entry entry;
		uint8_t bytes[256]; /* 256 is maximum length possible */
	} u;
	static unsigned int offset = 0;
	size_t len;
	unsigned int i;
	uint8_t sum;

	/* Return cached result if avaiable */
	if ( smbios.address != UNULL )
		return 0;

	/* Try to find SMBIOS */
	for ( ; offset < 0x10000 ; offset += 0x10 ) {

		/* Read start of header and verify signature */
		copy_from_real ( &u.entry, BIOS_SEG, offset,
				 sizeof ( u.entry ));
		if ( u.entry.signature != SMBIOS_SIGNATURE )
			continue;

		/* Read whole header and verify checksum */
		len = u.entry.len;
		copy_from_real ( &u.bytes, BIOS_SEG, offset, len );
		for ( i = 0 , sum = 0 ; i < len ; i++ ) {
			sum += u.bytes[i];
		}
		if ( sum != 0 ) {
			DBG ( "SMBIOS at %04x:%04x has bad checksum %02x\n",
			      BIOS_SEG, offset, sum );
			continue;
		}

		/* Fill result structure */
		DBG ( "Found SMBIOS entry point at %04x:%04x\n",
		      BIOS_SEG, offset );
		smbios.address = phys_to_user ( u.entry.smbios_address );
		smbios.len = u.entry.smbios_len;
		smbios.count = u.entry.smbios_count;
		return 0;
	}

	DBG ( "No SMBIOS found\n" );
	return -ENODEV;
}

/**
 * Find SMBIOS strings terminator
 *
 * @v offset		Offset to start of strings
 * @ret offset		Offset to strings terminator, or 0 if not found
 */
static size_t find_strings_terminator ( size_t offset ) {
	size_t max_offset = ( smbios.len - 2 );
	uint16_t nulnul;

	for ( ; offset <= max_offset ; offset++ ) {
		copy_from_user ( &nulnul, smbios.address, offset, 2 );
		if ( nulnul == 0 )
			return ( offset + 1 );
	}
	return 0;
}

/**
 * Find specific structure type within SMBIOS
 *
 * @v type		Structure type to search for
 * @v structure		SMBIOS structure descriptor to fill in
 * @ret rc		Return status code
 */
int find_smbios_structure ( unsigned int type,
			    struct smbios_structure *structure ) {
	unsigned int count = 0;
	size_t offset = 0;
	size_t strings_offset;
	size_t terminator_offset;
	int rc;

	/* Find SMBIOS */
	if ( ( rc = find_smbios() ) != 0 )
		return rc;

	/* Scan through list of structures */
	while ( ( ( offset + sizeof ( structure->header ) ) < smbios.len )
		&& ( count < smbios.count ) ) {

		/* Read next SMBIOS structure header */
		copy_from_user ( &structure->header, smbios.address, offset,
				 sizeof ( structure->header ) );

		/* Determine start and extent of strings block */
		strings_offset = ( offset + structure->header.len );
		if ( strings_offset > smbios.len ) {
			DBG ( "SMBIOS structure at offset %zx with length "
			      "%x extends beyond SMBIOS\n", offset,
			      structure->header.len );
			return -ENOENT;
		}
		terminator_offset = find_strings_terminator ( strings_offset );
		if ( ! terminator_offset ) {
			DBG ( "SMBIOS structure at offset %zx has "
			      "unterminated strings section\n", offset );
			return -ENOENT;
		}
		structure->strings_len = ( terminator_offset - strings_offset);

		DBG ( "SMBIOS structure at offset %zx has type %d, length %x, "
		      "strings length %zx\n", offset, structure->header.type,
		      structure->header.len, structure->strings_len );

		/* If this is the structure we want, return */
		if ( structure->header.type == type ) {
			structure->offset = offset;
			return 0;
		}

		/* Move to next SMBIOS structure */
		offset = ( terminator_offset + 1 );
		count++;
	}

	DBG ( "SMBIOS structure type %d not found\n", type );
	return -ENOENT;
}

/**
 * Copy SMBIOS structure
 *
 * @v structure		SMBIOS structure descriptor
 * @v data		Buffer to hold SMBIOS structure
 * @v len		Length of buffer
 * @ret rc		Return status code
 */
int read_smbios_structure ( struct smbios_structure *structure,
			    void *data, size_t len ) {

	assert ( smbios.address != UNULL );

	if ( len > structure->header.len )
		len = structure->header.len;
	copy_from_user ( data, smbios.address, structure->offset, len );
	return 0;
}

/**
 * Find indexed string within SMBIOS structure
 *
 * @v structure		SMBIOS structure descriptor
 * @v index		String index
 * @v data		Buffer for string
 * @v len		Length of string buffer
 * @ret rc		Length of string, or negative error
 */
int read_smbios_string ( struct smbios_structure *structure,
			 unsigned int index, void *data, size_t len ) {
	size_t strings_start = ( structure->offset + structure->header.len );
	size_t strings_end = ( strings_start + structure->strings_len );
	size_t offset;
	size_t string_len;

	assert ( smbios.address != UNULL );

	/* String numbers start at 1 (0 is used to indicate "no string") */
	if ( ! index )
		return -ENOENT;

	for ( offset = strings_start ; offset < strings_end ;
	      offset += ( string_len + 1 ) ) {
		/* Get string length.  This is known safe, since the
		 * smbios_strings struct is constructed so as to
		 * always end on a string boundary.
		 */
		string_len = strlen_user ( smbios.address, offset );
		if ( --index == 0 ) {
			/* Copy string, truncating as necessary. */
			if ( len > string_len )
				len = string_len;
			copy_from_user ( data, smbios.address, offset, len );
			return string_len;
		}
	}

	DBG ( "SMBIOS string index %d not found\n", index );
	return -ENOENT;
}
