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
 * @v table		Any ACPI table
 * @ret checksum	0 if checksum is good
 */
static uint8_t acpi_checksum ( userptr_t table ) {
	struct acpi_header acpi;
	uint8_t sum = 0;
	uint8_t data = 0;
	unsigned int i;

	/* Read table length */
	copy_from_user ( &acpi.length, table,
			 offsetof ( typeof ( acpi ), length ),
			 sizeof ( acpi.length ) );

	/* Compute checksum */
	for ( i = 0 ; i < le32_to_cpu ( acpi.length ) ; i++ ) {
		copy_from_user ( &data, table, i, sizeof ( data ) );
		sum += data;
	}

	return sum;
}

/**
 * Fix up ACPI table checksum
 *
 * @v acpi		ACPI table header
 */
void acpi_fix_checksum ( struct acpi_header *acpi ) {

	/* Update checksum */
	acpi->checksum -= acpi_checksum ( virt_to_user ( acpi ) );
}

/**
 * Locate ACPI table
 *
 * @v signature		Requested table signature
 * @v index		Requested index of table with this signature
 * @ret table		Table, or UNULL if not found
 */
userptr_t acpi_table ( uint32_t signature, unsigned int index ) {

	return ( *acpi_finder ) ( signature, index );
}

/**
 * Locate ACPI table via RSDT
 *
 * @v signature		Requested table signature
 * @v index		Requested index of table with this signature
 * @ret table		Table, or UNULL if not found
 */
userptr_t acpi_find_via_rsdt ( uint32_t signature, unsigned int index ) {
	struct acpi_header acpi;
	struct acpi_rsdt *rsdtab;
	typeof ( rsdtab->entry[0] ) entry;
	userptr_t rsdt;
	userptr_t table;
	size_t len;
	unsigned int count;
	unsigned int i;

	/* Locate RSDT */
	rsdt = acpi_find_rsdt();
	if ( ! rsdt ) {
		DBG ( "RSDT not found\n" );
		return UNULL;
	}

	/* Read RSDT header */
	copy_from_user ( &acpi, rsdt, 0, sizeof ( acpi ) );
	if ( acpi.signature != cpu_to_le32 ( RSDT_SIGNATURE ) ) {
		DBGC ( colour, "RSDT %#08lx has invalid signature:\n",
		       user_to_phys ( rsdt, 0 ) );
		DBGC_HDA ( colour, user_to_phys ( rsdt, 0 ), &acpi,
			   sizeof ( acpi ) );
		return UNULL;
	}
	len = le32_to_cpu ( acpi.length );
	if ( len < sizeof ( rsdtab->acpi ) ) {
		DBGC ( colour, "RSDT %#08lx has invalid length:\n",
		       user_to_phys ( rsdt, 0 ) );
		DBGC_HDA ( colour, user_to_phys ( rsdt, 0 ), &acpi,
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

		/* Check table integrity */
		if ( acpi_checksum ( table ) != 0 ) {
			DBGC ( colour, "RSDT %#08lx found %s with bad "
			       "checksum at %08lx\n", user_to_phys ( rsdt, 0 ),
			       acpi_name ( signature ),
			       user_to_phys ( table, 0 ) );
			break;
		}

		DBGC ( colour, "RSDT %#08lx found %s at %08lx\n",
		       user_to_phys ( rsdt, 0 ), acpi_name ( signature ),
		       user_to_phys ( table, 0 ) );
		return table;
	}

	DBGC ( colour, "RSDT %#08lx could not find %s\n",
	       user_to_phys ( rsdt, 0 ), acpi_name ( signature ) );
	return UNULL;
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
static int acpi_zsdt ( userptr_t zsdt, uint32_t signature, void *data,
		       int ( * extract ) ( userptr_t zsdt, size_t len,
					   size_t offset, void *data ) ) {
	struct acpi_header acpi;
	uint32_t buf;
	size_t offset;
	size_t len;
	int rc;

	/* Read table header */
	copy_from_user ( &acpi, zsdt, 0, sizeof ( acpi ) );
	len = le32_to_cpu ( acpi.length );

	/* Locate signature */
	for ( offset = sizeof ( acpi ) ;
	      ( ( offset + sizeof ( buf ) /* signature */ ) < len ) ;
	      offset++ ) {

		/* Check signature */
		copy_from_user ( &buf, zsdt, offset, sizeof ( buf ) );
		if ( buf != cpu_to_le32 ( signature ) )
			continue;
		DBGC ( zsdt, "DSDT/SSDT %#08lx found %s at offset %#zx\n",
		       user_to_phys ( zsdt, 0 ), acpi_name ( signature ),
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
		   int ( * extract ) ( userptr_t zsdt, size_t len,
				       size_t offset, void *data ) ) {
	struct acpi_fadt fadtab;
	userptr_t fadt;
	userptr_t dsdt;
	userptr_t ssdt;
	unsigned int i;
	int rc;

	/* Try DSDT first */
	fadt = acpi_table ( FADT_SIGNATURE, 0 );
	if ( fadt ) {
		copy_from_user ( &fadtab, fadt, 0, sizeof ( fadtab ) );
		dsdt = phys_to_user ( fadtab.dsdt );
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
