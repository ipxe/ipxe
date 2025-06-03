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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <ipxe/uaccess.h>
#include <ipxe/smbios.h>

/** @file
 *
 * System Management BIOS
 *
 */

/** SMBIOS entry point descriptor */
static struct smbios smbios = {
	.address = NULL,
};

/**
 * Calculate SMBIOS entry point structure checksum
 *
 * @v start		Start address of region
 * @v len		Length of entry point structure
 * @ret sum		Byte checksum
 */
static uint8_t smbios_checksum ( const void *start, size_t len ) {
	const uint8_t *byte = start;
	uint8_t sum = 0;

	/* Compute checksum */
	while ( len-- )
		sum += *(byte++);

	return sum;
}

/**
 * Scan for SMBIOS 32-bit entry point structure
 *
 * @v start		Start address of region to scan
 * @v len		Length of region to scan
 * @ret entry		SMBIOS entry point structure, or NULL if not found
 */
const struct smbios_entry * find_smbios_entry ( const void *start,
						size_t len ) {
	static size_t offset = 0; /* Avoid repeated attempts to locate SMBIOS */
	const struct smbios_entry *entry;
	uint8_t sum;

	/* Try to find SMBIOS */
	for ( ; ( offset + sizeof ( *entry ) ) <= len ; offset += 0x10 ) {

		/* Verify signature */
		entry = ( start + offset );
		if ( entry->signature != SMBIOS_SIGNATURE )
			continue;

		/* Verify length */
		if ( ( entry->len < sizeof ( *entry ) ) ||
		     ( ( offset + entry->len ) > len ) ) {
			DBGC ( &smbios, "SMBIOS at %#08lx has bad length "
			       "%#02x\n", virt_to_phys ( entry ), entry->len );
			continue;
		}

		/* Verify checksum */
		if ( ( sum = smbios_checksum ( entry, entry->len ) ) != 0 ) {
			DBGC ( &smbios, "SMBIOS at %#08lx has bad checksum "
			       "%#02x\n", virt_to_phys ( entry ), sum );
			continue;
		}

		/* Fill result structure */
		DBGC ( &smbios, "Found SMBIOS v%d.%d entry point at %#08lx\n",
		       entry->major, entry->minor, virt_to_phys ( entry ) );
		return entry;
	}

	DBGC ( &smbios, "No SMBIOS found\n" );
	return NULL;
}

/**
 * Scan for SMBIOS 64-bit entry point structure
 *
 * @v start		Start address of region to scan
 * @v len		Length of region to scan
 * @ret entry		SMBIOS entry point structure, or NULL if not found
 */
const struct smbios3_entry * find_smbios3_entry ( const void *start,
						  size_t len ) {
	static size_t offset = 0; /* Avoid repeated attempts to locate SMBIOS */
	const struct smbios3_entry *entry;
	uint8_t sum;

	/* Try to find SMBIOS */
	for ( ; ( offset + sizeof ( *entry ) ) <= len ; offset += 0x10 ) {

		/* Verify signature */
		entry = ( start + offset );
		if ( entry->signature != SMBIOS3_SIGNATURE )
			continue;

		/* Verify length */
		if ( ( entry->len < sizeof ( *entry ) ) ||
		     ( ( offset + entry->len ) > len ) ) {
			DBGC ( &smbios, "SMBIOS at %#08lx has bad length "
			       "%#02x\n", virt_to_phys ( entry ), entry->len );
			continue;
		}

		/* Verify checksum */
		if ( ( sum = smbios_checksum ( entry, entry->len ) ) != 0 ) {
			DBGC ( &smbios, "SMBIOS3 at %#08lx has bad checksum "
			       "%#02x\n", virt_to_phys ( entry ), sum );
			continue;
		}

		/* Fill result structure */
		DBGC ( &smbios, "Found SMBIOS3 v%d.%d entry point at %#08lx\n",
		       entry->major, entry->minor, virt_to_phys ( entry ) );
		return entry;
	}

	DBGC ( &smbios, "No SMBIOS3 found\n" );
	return NULL;
}

/**
 * Find SMBIOS strings terminator
 *
 * @v offset		Offset to start of strings
 * @ret offset		Offset to strings terminator, or 0 if not found
 */
static size_t find_strings_terminator ( size_t offset ) {
	const uint16_t *nulnul __attribute__ (( aligned ( 1 ) ));

	/* Sanity checks */
	assert ( smbios.address != NULL );

	/* Check for presence of terminating empty string */
	for ( ; ( offset + sizeof ( *nulnul ) ) <= smbios.len ; offset++ ) {
		nulnul = ( smbios.address + offset );
		if ( *nulnul == 0 )
			return ( offset + 1 );
	}
	return 0;
}

/**
 * Find specific structure type within SMBIOS
 *
 * @v type		Structure type to search for
 * @v instance		Instance of this type of structure
 * @ret structure	SMBIOS structure header, or NULL if not found
 */
const struct smbios_header * smbios_structure ( unsigned int type,
						unsigned int instance ) {
	const struct smbios_header *structure;
	unsigned int count = 0;
	size_t offset = 0;
	size_t strings_offset;
	size_t terminator_offset;
	size_t strings_len;
	int rc;

	/* Find SMBIOS */
	if ( ( smbios.address == NULL ) &&
	     ( ( rc = find_smbios ( &smbios ) ) != 0 ) )
		return NULL;
	assert ( smbios.address != NULL );

	/* Scan through list of structures */
	while ( ( ( offset + sizeof ( *structure ) ) < smbios.len ) &&
		( ( smbios.count == 0 ) || ( count < smbios.count ) ) ) {

		/* Access next SMBIOS structure header */
		structure = ( smbios.address + offset );

		/* Determine start and extent of strings block */
		strings_offset = ( offset + structure->len );
		if ( strings_offset > smbios.len ) {
			DBGC ( &smbios, "SMBIOS structure at offset %#zx "
			       "with length %#x extends beyond SMBIOS\n",
			       offset, structure->len );
			return NULL;
		}
		terminator_offset = find_strings_terminator ( strings_offset );
		if ( ! terminator_offset ) {
			DBGC ( &smbios, "SMBIOS structure at offset %#zx has "
			       "unterminated strings section\n", offset );
			return NULL;
		}
		strings_len = ( terminator_offset - strings_offset);
		DBGC ( &smbios, "SMBIOS structure at offset %#zx has type %d, "
		       "length %#x, strings length %#zx\n", offset,
		       structure->type, structure->len, strings_len );

		/* Stop if we have reached an end-of-table marker */
		if ( ( smbios.count == 0 ) &&
		     ( structure->type == SMBIOS_TYPE_END ) )
			break;

		/* If this is the structure we want, return */
		if ( ( structure->type == type ) &&
		     ( instance-- == 0 ) ) {
			return structure;
		}

		/* Move to next SMBIOS structure */
		offset = ( terminator_offset + 1 );
		count++;
	}

	DBGC ( &smbios, "SMBIOS structure type %d not found\n", type );
	return NULL;
}

/**
 * Get indexed string within SMBIOS structure
 *
 * @v structure		SMBIOS structure header
 * @v index		String index
 * @ret string		SMBIOS string, or NULL if not fond
 */
const char * smbios_string ( const struct smbios_header *structure,
			     unsigned int index ) {
	const char *string;
	unsigned int i;
	size_t len;

	/* Sanity check */
	assert ( smbios.address != NULL );

	/* Step through strings */
	string = ( ( ( const void * ) structure ) + structure->len );
	for ( i = index ; i-- ; ) {
		/* Get string length.  This is known safe, since we
		 * check for the empty-string terminator in
		 * smbios_structure().
		 */
		len = strlen ( string );
		if ( ! len )
			break;
		if ( i == 0 )
			return string;
		string += ( len + 1 /* NUL */ );
	}

	DBGC ( &smbios, "SMBIOS string index %d not found\n", index );
	return NULL;
}

/**
 * Get SMBIOS version
 *
 * @ret version		Version, or negative error
 */
int smbios_version ( void ) {
	int rc;

	/* Find SMBIOS */
	if ( ( smbios.address == NULL ) &&
	     ( ( rc = find_smbios ( &smbios ) ) != 0 ) )
		return rc;
	assert ( smbios.address != NULL );

	return smbios.version;
}

/**
 * Clear SMBIOS entry point descriptor
 *
 */
void smbios_clear ( void ) {

	/* Clear address */
	smbios.address = NULL;
}
