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

#include <string.h>
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

/** Colour for debug messages */
#define colour FADT_SIGNATURE

/** ACPI table finder
 *
 * May be overridden at link time to inject tables for testing.
 */
typeof ( acpi_find ) *acpi_finder __attribute__ (( weak )) = acpi_find;

/******************************************************************************
 *
 * Utility functions
 *
 ******************************************************************************
 */

/**
 * Compute ACPI table checksum
 *
 * @v acpi		Any ACPI table header
 * @ret checksum	0 if checksum is good
 */
static uint8_t acpi_checksum ( const struct acpi_header *acpi ) {
	const uint8_t *byte = ( ( const void * ) acpi );
	size_t len = le32_to_cpu ( acpi->length );
	uint8_t sum = 0;

	/* Compute checksum */
	while ( len-- )
		sum += *(byte++);

	return sum;
}

/**
 * Fix up ACPI table checksum
 *
 * @v acpi		ACPI table header
 */
void acpi_fix_checksum ( struct acpi_header *acpi ) {

	/* Update checksum */
	acpi->checksum -= acpi_checksum ( acpi );
}

/**
 * Locate ACPI table
 *
 * @v signature		Requested table signature
 * @v index		Requested index of table with this signature
 * @ret table		Table, or NULL if not found
 */
const struct acpi_header * acpi_table ( uint32_t signature,
					unsigned int index ) {

	return ( *acpi_finder ) ( signature, index );
}

/**
 * Locate ACPI table via RSDT
 *
 * @v signature		Requested table signature
 * @v index		Requested index of table with this signature
 * @ret table		Table, or NULL if not found
 */
const struct acpi_header * acpi_find_via_rsdt ( uint32_t signature,
						unsigned int index ) {
	const struct acpi_rsdt *rsdt;
	const struct acpi_header *table;
	size_t len;
	unsigned int count;
	unsigned int i;

	/* Locate RSDT */
	rsdt = acpi_find_rsdt();
	if ( ! rsdt ) {
		DBG ( "RSDT not found\n" );
		return NULL;
	}

	/* Read RSDT header */
	if ( rsdt->acpi.signature != cpu_to_le32 ( RSDT_SIGNATURE ) ) {
		DBGC ( colour, "RSDT %#08lx has invalid signature:\n",
		       virt_to_phys ( rsdt ) );
		DBGC_HDA ( colour, virt_to_phys ( rsdt ), &rsdt->acpi,
			   sizeof ( rsdt->acpi ) );
		return NULL;
	}
	len = le32_to_cpu ( rsdt->acpi.length );
	if ( len < sizeof ( rsdt->acpi ) ) {
		DBGC ( colour, "RSDT %#08lx has invalid length:\n",
		       virt_to_phys ( rsdt ) );
		DBGC_HDA ( colour, virt_to_phys ( rsdt ), &rsdt->acpi,
			   sizeof ( rsdt->acpi ) );
		return NULL;
	}

	/* Calculate number of entries */
	count = ( ( len - sizeof ( rsdt->acpi ) ) /
		  sizeof ( rsdt->entry[0] ) );

	/* Search through entries */
	for ( i = 0 ; i < count ; i++ ) {

		/* Read table header */
		table = phys_to_virt ( rsdt->entry[i] );

		/* Check table signature */
		if ( table->signature != cpu_to_le32 ( signature ) )
			continue;

		/* Check index */
		if ( index-- )
			continue;

		/* Check table integrity */
		if ( acpi_checksum ( table ) != 0 ) {
			DBGC ( colour, "RSDT %#08lx found %s with bad "
			       "checksum at %#08lx\n", virt_to_phys ( rsdt ),
			       acpi_name ( signature ),
			       virt_to_phys ( table ) );
			break;
		}

		DBGC ( colour, "RSDT %#08lx found %s at %#08lx\n",
		       virt_to_phys ( rsdt ), acpi_name ( signature ),
		       virt_to_phys ( table ) );
		return table;
	}

	DBGC ( colour, "RSDT %#08lx could not find %s\n",
	       virt_to_phys ( rsdt ), acpi_name ( signature ) );
	return NULL;
}

/**
 * Extract value from DSDT/SSDT
 *
 * @v zsdt		DSDT or SSDT
 * @v signature		Signature (e.g. "_S5_")
 * @v data		Data buffer
 * @v extract		Extraction method
 * @ret rc		Return status code
 */
static int acpi_zsdt ( const struct acpi_header *zsdt,
		       uint32_t signature, void *data,
		       int ( * extract ) ( const struct acpi_header *zsdt,
					   size_t len, size_t offset,
					   void *data ) ) {
	uint32_t buf;
	size_t offset;
	size_t len;
	int rc;

	/* Read table header */
	len = le32_to_cpu ( zsdt->length );

	/* Locate signature */
	for ( offset = sizeof ( *zsdt ) ;
	      ( ( offset + sizeof ( buf ) /* signature */ ) < len ) ;
	      offset++ ) {

		/* Check signature */
		memcpy ( &buf, ( ( ( const void * ) zsdt ) + offset ),
			 sizeof ( buf ) );
		if ( buf != cpu_to_le32 ( signature ) )
			continue;
		DBGC ( zsdt, "DSDT/SSDT %#08lx found %s at offset %#zx\n",
		       virt_to_phys ( zsdt ), acpi_name ( signature ),
		       offset );

		/* Attempt to extract data */
		if ( ( rc = extract ( zsdt, len, offset, data ) ) == 0 )
			return 0;
	}

	return -ENOENT;
}

/**
 * Extract value from DSDT/SSDT
 *
 * @v signature		Signature (e.g. "_S5_")
 * @v data		Data buffer
 * @v extract		Extraction method
 * @ret rc		Return status code
 */
int acpi_extract ( uint32_t signature, void *data,
		   int ( * extract ) ( const struct acpi_header *zsdt,
				       size_t len, size_t offset,
				       void *data ) ) {
	const struct acpi_fadt *fadt;
	const struct acpi_header *dsdt;
	const struct acpi_header *ssdt;
	unsigned int i;
	int rc;

	/* Try DSDT first */
	fadt = container_of ( acpi_table ( FADT_SIGNATURE, 0 ),
			      struct acpi_fadt, acpi );
	if ( fadt ) {
		dsdt = phys_to_virt ( fadt->dsdt );
		if ( ( rc = acpi_zsdt ( dsdt, signature, data,
					extract ) ) == 0 )
			return 0;
	}

	/* Try all SSDTs */
	for ( i = 0 ; ; i++ ) {
		ssdt = acpi_table ( SSDT_SIGNATURE, i );
		if ( ! ssdt )
			break;
		if ( ( rc = acpi_zsdt ( ssdt, signature, data,
					extract ) ) == 0 )
			return 0;
	}

	DBGC ( colour, "ACPI could not find \"%s\"\n",
	       acpi_name ( signature ) );
	return -ENOENT;
}

/******************************************************************************
 *
 * Descriptors
 *
 ******************************************************************************
 */

/**
 * Add ACPI descriptor
 *
 * @v desc		ACPI descriptor
 */
void acpi_add ( struct acpi_descriptor *desc ) {

	/* Add to list of descriptors */
	ref_get ( desc->refcnt );
	list_add_tail ( &desc->list, &desc->model->descs );
}

/**
 * Remove ACPI descriptor
 *
 * @v desc		ACPI descriptor
 */
void acpi_del ( struct acpi_descriptor *desc ) {

	/* Remove from list of descriptors */
	list_check_contains_entry ( desc, &desc->model->descs, list );
	list_del ( &desc->list );
	ref_put ( desc->refcnt );
}

/**
 * Get object's ACPI descriptor
 *
 * @v intf		Interface
 * @ret desc		ACPI descriptor, or NULL
 */
struct acpi_descriptor * acpi_describe ( struct interface *intf ) {
	struct interface *dest;
	acpi_describe_TYPE ( void * ) *op =
		intf_get_dest_op ( intf, acpi_describe, &dest );
	void *object = intf_object ( dest );
	struct acpi_descriptor *desc;

	if ( op ) {
		desc = op ( object );
	} else {
		desc = NULL;
	}

	intf_put ( dest );
	return desc;
}

/**
 * Install ACPI tables
 *
 * @v install		Table installation method
 * @ret rc		Return status code
 */
int acpi_install ( int ( * install ) ( struct acpi_header *acpi ) ){
	struct acpi_model *model;
	int rc;

	for_each_table_entry ( model, ACPI_MODELS ) {
		if ( ( rc = model->install ( install ) ) != 0 )
			return rc;
	}

	return 0;
}
