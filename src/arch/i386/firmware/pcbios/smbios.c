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
#include <stdio.h>
#include <errno.h>
#include <gpxe/uaccess.h>
#include <gpxe/uuid.h>
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
	uint8_t length;
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
	uint16_t smbios_length;
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
	size_t length;
	/** Number of SMBIOS structures */
	unsigned int count;
};

/**
 * SMBIOS strings descriptor
 *
 * This is returned as part of the search for an SMBIOS structure, and
 * contains the information needed for extracting the strings within
 * the "unformatted" portion of the structure.
 */
struct smbios_strings {
	/** Start of strings data */
	userptr_t data;
	/** Length of strings data */
	size_t length;
};

/**
 * Find SMBIOS
 *
 * @ret smbios		SMBIOS entry point descriptor, or NULL if not found
 */
static struct smbios * find_smbios ( void ) {
	static struct smbios smbios = {
		.address = UNULL,
	};
	union {
		struct smbios_entry entry;
		uint8_t bytes[256]; /* 256 is maximum length possible */
	} u;
	unsigned int offset;
	size_t len;
	unsigned int i;
	uint8_t sum;

	/* Return cached result if available */
	if ( smbios.address != UNULL )
		return &smbios;

	/* Try to find SMBIOS */
	for ( offset = 0 ; offset < 0x10000 ; offset += 0x10 ) {

		/* Read start of header and verify signature */
		copy_from_real ( &u.entry, BIOS_SEG, offset,
				 sizeof ( u.entry ));
		if ( u.entry.signature != SMBIOS_SIGNATURE )
			continue;

		/* Read whole header and verify checksum */
		len = u.entry.length;
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
		smbios.length = u.entry.smbios_length;
		smbios.count = u.entry.smbios_count;
		return &smbios;
	}

	DBG ( "No SMBIOS found\n" );
	return NULL;
}

/**
 * Find SMBIOS strings terminator
 *
 * @v smbios		SMBIOS entry point descriptor
 * @v offset		Offset to start of strings
 * @ret offset		Offset to strings terminator, or 0 if not found
 */
static size_t find_strings_terminator ( struct smbios *smbios,
					size_t offset ) {
	size_t max_offset = ( smbios->length - 2 );
	uint16_t nulnul;

	for ( ; offset <= max_offset ; offset++ ) {
		copy_from_user ( &nulnul, smbios->address, offset, 2 );
		if ( nulnul == 0 )
			return ( offset + 1 );
	}
	return 0;
}

/**
 * Find specific structure type within SMBIOS
 *
 * @v type		Structure type to search for
 * @v structure		Buffer to fill in with structure
 * @v length		Length of buffer
 * @v strings		Strings descriptor to fill in, or NULL
 * @ret rc		Return status code
 */
int find_smbios_structure ( unsigned int type, void *structure,
			    size_t length, struct smbios_strings *strings ) {
	struct smbios *smbios;
	struct smbios_header header;
	struct smbios_strings temp_strings;
	unsigned int count = 0;
	size_t offset = 0;
	size_t strings_offset;
	size_t terminator_offset;

	/* Locate SMBIOS entry point */
	if ( ! ( smbios = find_smbios() ) )
		return -ENOENT;

	/* Ensure that we have a usable strings descriptor buffer */
	if ( ! strings )
		strings = &temp_strings;

	/* Scan through list of structures */
	while ( ( ( offset + sizeof ( header ) ) < smbios->length ) &&
		( count < smbios->count ) ) {

		/* Read next SMBIOS structure header */
		copy_from_user ( &header, smbios->address, offset,
				 sizeof ( header ) );

		/* Determine start and extent of strings block */
		strings_offset = ( offset + header.length );
		if ( strings_offset > smbios->length ) {
			DBG ( "SMBIOS structure at offset %zx with length "
			      "%zx extends beyond SMBIOS\n", offset,
			      header.length );
			return -ENOENT;
		}
		terminator_offset =
			find_strings_terminator ( smbios, strings_offset );
		if ( ! terminator_offset ) {
			DBG ( "SMBIOS structure at offset %zx has "
			      "unterminated strings section\n", offset );
			return -ENOENT;
		}
		strings->data = userptr_add ( smbios->address,
					      strings_offset );
		strings->length = ( terminator_offset - strings_offset );

		DBG ( "SMBIOS structure at offset %zx has type %d, "
		      "length %zx, strings length %zx\n",
		      offset, header.type, header.length, strings->length );

		/* If this is the structure we want, return */
		if ( header.type == type ) {
			if ( length > header.length )
				length = header.length;
			copy_from_user ( structure, smbios->address,
					 offset, length );
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
 * Find indexed string within SMBIOS structure
 *
 * @v strings		SMBIOS strings descriptor
 * @v index		String index
 * @v buffer		Buffer for string
 * @v length		Length of string buffer
 * @ret rc		Return status code
 */
int find_smbios_string ( struct smbios_strings *strings, unsigned int index,
			 char *buffer, size_t length ) {
	size_t offset = 0;
	size_t string_len;

	/* Zero buffer.  This ensures that a valid NUL terminator is
	 * always present (unless length==0).
	 */
	memset ( buffer, 0, length );
	   
	/* String numbers start at 1 (0 is used to indicate "no string") */
	if ( ! index )
		return 0;

	while ( offset < strings->length ) {
		/* Get string length.  This is known safe, since the
		 * smbios_strings struct is constructed so as to
		 * always end on a string boundary.
		 */
		string_len = strlen_user ( strings->data, offset );
		if ( --index == 0 ) {
			/* Copy string, truncating as necessary. */
			if ( string_len >= length )
				string_len = ( length - 1 );
			copy_from_user ( buffer, strings->data,
					 offset, string_len );
			return 0;
		}
		offset += ( string_len + 1 );
	}

	DBG ( "SMBIOS string index %d not found\n", index );
	return -ENOENT;
}

/**
 * Get UUID from SMBIOS
 *
 * @v uuid		UUID to fill in
 * @ret rc		Return status code
 */
int smbios_get_uuid ( union uuid *uuid ) {
	struct smbios_system_information sysinfo;
	int rc;

	if ( ( rc = find_smbios_structure ( SMBIOS_TYPE_SYSTEM_INFORMATION,
					    &sysinfo, sizeof ( sysinfo ),
					    NULL ) ) != 0 )
		return rc;

	memcpy ( uuid, sysinfo.uuid, sizeof ( *uuid ) );
	DBG ( "SMBIOS found UUID %s\n", uuid_ntoa ( uuid ) );

	return 0;
}
