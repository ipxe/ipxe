/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <errno.h>
#include <byteswap.h>
#include <ipxe/uaccess.h>
#include <ipxe/acpi.h>
#include <ipxe/interface.h>

/** @file
 *
 * ACPI support functions
 *
 */

/******************************************************************************
 *
 * Utility functions
 *
 ******************************************************************************
 */

/**
 * Transcribe ACPI table signature (for debugging)
 *
 * @v signature		ACPI table signature
 * @ret name		ACPI table signature name
 */
static const char * acpi_name ( uint32_t signature ) {
	static union {
		uint32_t signature;
		char name[5];
	} u;

	u.signature = cpu_to_le32 ( signature );
	return u.name;
}

/**
 * Fix up ACPI table checksum
 *
 * @v acpi		ACPI table header
 */
void acpi_fix_checksum ( struct acpi_description_header *acpi ) {
	unsigned int i = 0;
	uint8_t sum = 0;

	for ( i = 0 ; i < acpi->length ; i++ ) {
		sum += *( ( ( uint8_t * ) acpi ) + i );
	}
	acpi->checksum -= sum;
}

/**
 * Locate ACPI root system description table within a memory range
 *
 * @v start		Start address to search
 * @v len		Length to search
 * @ret rsdt		ACPI root system description table, or UNULL
 */
static userptr_t acpi_find_rsdt_range ( userptr_t start, size_t len ) {
	static const char signature[8] = RSDP_SIGNATURE;
	struct acpi_rsdp rsdp;
	userptr_t rsdt;
	size_t offset;
	uint8_t sum;
	unsigned int i;

	/* Search for RSDP */
	for ( offset = 0 ; ( ( offset + sizeof ( rsdp ) ) < len ) ;
	      offset += RSDP_STRIDE ) {

		/* Check signature and checksum */
		copy_from_user ( &rsdp, start, offset, sizeof ( rsdp ) );
		if ( memcmp ( rsdp.signature, signature,
			      sizeof ( signature ) ) != 0 )
			continue;
		for ( sum = 0, i = 0 ; i < sizeof ( rsdp ) ; i++ )
			sum += *( ( ( uint8_t * ) &rsdp ) + i );
		if ( sum != 0 )
			continue;

		/* Extract RSDT */
		rsdt = phys_to_user ( le32_to_cpu ( rsdp.rsdt ) );
		DBGC ( rsdt, "RSDT %#08lx found via RSDP %#08lx\n",
		       user_to_phys ( rsdt, 0 ),
		       user_to_phys ( start, offset ) );
		return rsdt;
	}

	return UNULL;
}

/**
 * Locate ACPI root system description table
 *
 * @v ebda		Extended BIOS data area, or UNULL
 * @ret rsdt		ACPI root system description table, or UNULL
 */
userptr_t acpi_find_rsdt ( userptr_t ebda ) {
	userptr_t rsdt;

	/* Search EBDA, if applicable */
	if ( ebda ) {
		rsdt = acpi_find_rsdt_range ( ebda, RSDP_EBDA_LEN );
		if ( rsdt )
			return rsdt;
	}

	/* Search fixed BIOS area */
	rsdt = acpi_find_rsdt_range ( phys_to_user ( RSDP_BIOS_START ),
				      RSDP_BIOS_LEN );
	if ( rsdt )
		return rsdt;

	return UNULL;
}

/**
 * Locate ACPI table
 *
 * @v rsdt		ACPI root system description table
 * @v signature		Requested table signature
 * @v index		Requested index of table with this signature
 * @ret table		Table, or UNULL if not found
 */
userptr_t acpi_find ( userptr_t rsdt, uint32_t signature, unsigned int index ) {
	struct acpi_description_header acpi;
	struct acpi_rsdt *rsdtab;
	typeof ( rsdtab->entry[0] ) entry;
	userptr_t table;
	size_t len;
	unsigned int count;
	unsigned int i;

	/* Read RSDT header */
	copy_from_user ( &acpi, rsdt, 0, sizeof ( acpi ) );
	if ( acpi.signature != cpu_to_le32 ( RSDT_SIGNATURE ) ) {
		DBGC ( rsdt, "RSDT %#08lx has invalid signature:\n",
		       user_to_phys ( rsdt, 0 ) );
		DBGC_HDA ( rsdt, user_to_phys ( rsdt, 0 ), &acpi,
			   sizeof ( acpi ) );
		return UNULL;
	}
	len = le32_to_cpu ( acpi.length );
	if ( len < sizeof ( rsdtab->acpi ) ) {
		DBGC ( rsdt, "RSDT %#08lx has invalid length:\n",
		       user_to_phys ( rsdt, 0 ) );
		DBGC_HDA ( rsdt, user_to_phys ( rsdt, 0 ), &acpi,
			   sizeof ( acpi ) );
		return UNULL;
	}

	/* Calculate number of entries */
	count = ( ( len - sizeof ( rsdtab->acpi ) ) / sizeof ( entry ) );

	/* Search through entries */
	for ( i = 0 ; i < count ; i++ ) {

		/* Get table address */
		copy_from_user ( &entry, rsdt,
				 offsetof ( typeof ( *rsdtab ), entry[i] ),
				 sizeof ( entry ) );

		/* Read table header */
		table = phys_to_user ( entry );
		copy_from_user ( &acpi.signature, table, 0,
				 sizeof ( acpi.signature ) );

		/* Check table signature */
		if ( acpi.signature != cpu_to_le32 ( signature ) )
			continue;

		/* Check index */
		if ( index-- )
			continue;

		DBGC ( rsdt, "RSDT %#08lx found %s at %08lx\n",
		       user_to_phys ( rsdt, 0 ), acpi_name ( signature ),
		       user_to_phys ( table, 0 ) );
		return table;
	}

	DBGC ( rsdt, "RSDT %#08lx could not find %s\n",
	       user_to_phys ( rsdt, 0 ), acpi_name ( signature ) );
	return UNULL;
}

/**
 * Extract \_Sx value from DSDT/SSDT
 *
 * @v zsdt		DSDT or SSDT
 * @v signature		Signature (e.g. "_S5_")
 * @ret sx		\_Sx value, or negative error
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
static int acpi_sx_zsdt ( userptr_t zsdt, uint32_t signature ) {
	struct acpi_description_header acpi;
	union {
		uint32_t dword;
		uint8_t byte[4];
	} buf;
	size_t offset;
	size_t len;
	unsigned int sx;
	uint8_t *byte;

	/* Read table header */
	copy_from_user ( &acpi, zsdt, 0, sizeof ( acpi ) );
	len = le32_to_cpu ( acpi.length );

	/* Locate signature */
	for ( offset = sizeof ( acpi ) ;
	      ( ( offset + sizeof ( buf ) /* signature */ + 3 /* pkg header */
		  + sizeof ( buf ) /* value */ ) < len ) ;
	      offset++ ) {

		/* Check signature */
		copy_from_user ( &buf, zsdt, offset, sizeof ( buf ) );
		if ( buf.dword != cpu_to_le32 ( signature ) )
			continue;
		DBGC ( zsdt, "DSDT/SSDT %#08lx found %s at offset %#zx\n",
		       user_to_phys ( zsdt, 0 ), acpi_name ( signature ),
		       offset );
		offset += sizeof ( buf );

		/* Read first four bytes of value */
		copy_from_user ( &buf, zsdt, ( offset + 3 /* pkg header */ ),
				 sizeof ( buf ) );
		DBGC ( zsdt, "DSDT/SSDT %#08lx found %s containing "
		       "%02x:%02x:%02x:%02x\n", user_to_phys ( zsdt, 0 ),
		       acpi_name ( signature ), buf.byte[0], buf.byte[1],
		       buf.byte[2], buf.byte[3] );

		/* Extract \Sx value.  There are three potential
		 * encodings that we might encounter:
		 *
		 * - SLP_TYPa, SLP_TYPb, rsvd, rsvd
		 *
		 * - <byteprefix>, SLP_TYPa, <byteprefix>, SLP_TYPb, ...
		 *
		 * - <dwordprefix>, SLP_TYPa, SLP_TYPb, 0, 0
		 *
		 * Since <byteprefix> and <dwordprefix> both have bit
		 * 3 set, and valid SLP_TYPx must have bit 3 clear
		 * (since SLP_TYPx is a 3-bit field), we can just skip
		 * any bytes with bit 3 set.
		 */
		byte = &buf.byte[0];
		if ( *byte & 0x08 )
			byte++;
		sx = *(byte++);
		if ( *byte & 0x08 )
			byte++;
		sx |= ( *byte << 8 );
		return sx;
	}

	return -ENOENT;
}

/**
 * Extract \_Sx value from DSDT/SSDT
 *
 * @v rsdt		ACPI root system description table
 * @v signature		Signature (e.g. "_S5_")
 * @ret sx		\_Sx value, or negative error
 */
int acpi_sx ( userptr_t rsdt, uint32_t signature ) {
	struct acpi_fadt fadtab;
	userptr_t fadt;
	userptr_t dsdt;
	userptr_t ssdt;
	unsigned int i;
	int sx;

	/* Try DSDT first */
	fadt = acpi_find ( rsdt, FADT_SIGNATURE, 0 );
	if ( fadt ) {
		copy_from_user ( &fadtab, fadt, 0, sizeof ( fadtab ) );
		dsdt = phys_to_user ( fadtab.dsdt );
		if ( ( sx = acpi_sx_zsdt ( dsdt, signature ) ) >= 0 )
			return sx;
	}

	/* Try all SSDTs */
	for ( i = 0 ; ; i++ ) {
		ssdt = acpi_find ( rsdt, SSDT_SIGNATURE, i );
		if ( ! ssdt )
			break;
		if ( ( sx = acpi_sx_zsdt ( ssdt, signature ) ) >= 0 )
			return sx;
	}

	DBGC ( rsdt, "RSDT %#08lx could not find \\_Sx \"%s\"\n",
	       user_to_phys ( rsdt, 0 ), acpi_name ( signature ) );
	return -ENOENT;
}

/******************************************************************************
 *
 * Interface methods
 *
 ******************************************************************************
 */

/**
 * Describe object in an ACPI table
 *
 * @v intf		Interface
 * @v acpi		ACPI table
 * @v len		Length of ACPI table
 * @ret rc		Return status code
 */
int acpi_describe ( struct interface *intf,
		    struct acpi_description_header *acpi, size_t len ) {
	struct interface *dest;
	acpi_describe_TYPE ( void * ) *op =
		intf_get_dest_op ( intf, acpi_describe, &dest );
	void *object = intf_object ( dest );
	int rc;

	if ( op ) {
		rc = op ( object, acpi, len );
	} else {
		/* Default is to fail to describe */
		rc = -EOPNOTSUPP;
	}

	intf_put ( dest );
	return rc;
}
