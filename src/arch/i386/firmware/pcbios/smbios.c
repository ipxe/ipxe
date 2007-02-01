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
#include <realmode.h>
#include <pnpbios.h>
#include <smbios.h>

/** @file
 *
 * System Management BIOS
 *
 */

/** Signature for an SMBIOS structure */
#define SMBIOS_SIGNATURE \
        ( ( '_' << 0 ) + ( 'S' << 8 ) + ( 'M' << 16 ) + ( '_' << 24 ) )

/** SMBIOS entry point */
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

/** An SMBIOS structure */
struct smbios {
	/** Type */
	uint8_t type;
	/** Length */
	uint8_t length;
	/** Handle */
	uint16_t handle;
} __attribute__ (( packed ));

struct smbios_system_information {
	struct smbios header;
	uint8_t manufacturer;
	uint8_t product;
	uint8_t version;
	uint8_t serial;
} __attribute__ (( packed ));

/**
 * Find SMBIOS
 *
 * @v emtry		SMBIOS entry point to fill in
 * @ret rc		Return status code
 */
static int find_smbios_entry ( struct smbios_entry *entry ) {
	union {
		struct smbios_entry entry;
		uint8_t bytes[256]; /* 256 is maximum length possible */
	} u;
	unsigned int offset;
	size_t len;
	unsigned int i;
	uint8_t sum = 0;

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
		for ( i = 0 ; i < len ; i++ ) {
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
		memcpy ( entry, &u.entry, sizeof ( *entry ) );
		return 0;
	}

	DBG ( "No SMBIOS found\n" );
	return -ENOENT;
}

/**
 * Find specific structure type within SMBIOS
 *
 * @v entry		SMBIOS entry point
 * @v type		Structure type
 * @v data		SMBIOS structure buffer to fill in
 * @ret rc		Return status code
 *
 * The buffer must be at least @c entry->max bytes in size.
 */
static int find_smbios ( struct smbios_entry *entry, unsigned int type,
			 void *data ) {
	struct smbios *smbios = data;
	userptr_t smbios_address = phys_to_user ( entry->smbios_address );
	unsigned int count = 0;
	size_t offset = 0;
	size_t frag_len;
	void *end;

	while ( ( offset < entry->smbios_length ) &&
		( count < entry->smbios_count ) ) {
		/* Read next SMBIOS structure */
		frag_len = ( entry->smbios_length - offset );
		if ( frag_len > entry->max )
			frag_len = entry->max;
		copy_from_user ( data, smbios_address, offset, frag_len );

		/* Sanity protection; ensure the last two bytes of the
		 * buffer are 0x00,0x00, just so that a terminator
		 * exists somewhere.  Also ensure that this lies
		 * outside the formatted area.
		 */
		*( ( uint16_t * ) ( data + entry->max - 2 ) ) = 0;
		if ( smbios->length > ( entry->max - 2 ) ) {
			DBG ( "Invalid SMBIOS structure length %zd\n",
			      smbios->length );
			return -ENOENT;
		}

		DBG ( "Found SMBIOS structure type %d at offset %zx\n",
		      smbios->type, offset );

		/* If this is the structure we want, return */
		if ( smbios->type == type )
			return 0;

		/* Find end of record.  This will always exist, thanks
		 * to our sanity check above.
		 */
		for ( end = ( data + smbios->length ) ;
		      end < ( data + entry->max ) ; end++ ) {
			if ( *( ( uint16_t * ) end ) == 0 ) {
				end += 2;
				break;
			}
		}

		offset += ( end - data );
		count++;
	}

	DBG ( "SMBIOS structure type %d not found\n", type );
	return -ENOENT;
}

/**
 * Find indexed string within SMBIOS structure
 *
 * @v data		SMBIOS structure
 * @v index		String index
 * @ret string		String, or NULL
 */
static const char * find_smbios_string ( void *data, unsigned int index ) {
	struct smbios *smbios = data;
	const char *string;
	size_t len;

	if ( ! index )
		return NULL;

	string = ( data + smbios->length );
	while ( --index ) {
		/* Move to next string */
		len = strlen ( string );
		if ( len == 0 ) {
			/* Reached premature end of string table */
			DBG ( "SMBIOS string index %d not found\n", index );
			return NULL;
		}
		string += ( len + 1 );
	}
	return string;
}

/**
 * Find SMBIOS serial number
 *
 * @v data		Buffer to fill
 * @v len		Length of buffer
 */
int find_smbios_serial ( void *data, size_t len ) {
	struct smbios_entry entry;
	const char *string;
	int rc;

	if ( ( rc = find_smbios_entry ( &entry ) ) != 0 )
		return rc;

	char buffer[entry.max];
	if ( ( rc = find_smbios ( &entry, 1, buffer ) ) != 0 )
		return rc;

	struct smbios_system_information *sysinfo = ( void * ) buffer;
	string = find_smbios_string ( buffer, sysinfo->serial );
	if ( ! string )
		return -ENOENT;

	DBG ( "Found serial number \"%s\"\n", string );
	snprintf ( data, len, "%s", string );
	return 0;
}
