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
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ipxe/linux_api.h>
#include <ipxe/linux.h>
#include <ipxe/list.h>
#include <ipxe/acpi.h>

/** ACPI sysfs directory */
#define ACPI_SYSFS_PREFIX "/sys/firmware/acpi/tables/"

/** A cached ACPI table */
struct linux_acpi_table {
	/** List of cached tables */
	struct list_head list;
	/** Signature */
	uint32_t signature;
	/** Index */
	unsigned int index;
	/** Cached data */
	void *data;
};

/** List of cached ACPI tables */
static LIST_HEAD ( linux_acpi_tables );

/**
 * Locate ACPI table
 *
 * @v signature		Requested table signature
 * @v index		Requested index of table with this signature
 * @ret table		Table, or UNULL if not found
 */
static userptr_t linux_acpi_find ( uint32_t signature, unsigned int index ) {
	struct linux_acpi_table *table;
	struct acpi_header header;
	union {
		uint32_t signature;
		char filename[5];
	} u;
	static const char prefix[] = ACPI_SYSFS_PREFIX;
	char filename[ sizeof ( prefix ) - 1 /* NUL */ + 4 /* signature */
		       + 3 /* "999" */ + 1 /* NUL */ ];
	ssize_t rlen;
	size_t len;
	void *data;
	int fd;

	/* Check for existing table */
	list_for_each_entry ( table, &linux_acpi_tables, list ) {
		if ( ( table->signature == signature ) &&
		     ( table->index == index ) )
			return virt_to_user ( table->data );
	}

	/* Allocate a new table */
	table = malloc ( sizeof ( *table ) );
	if ( ! table )
		goto err_alloc;
	table->signature = signature;
	table->index = index;

	/* Construct filename */
	memset ( &u, 0, sizeof ( u ) );
	u.signature = le32_to_cpu ( signature );
	snprintf ( filename, sizeof ( filename ), "%s%s%d", prefix,
		   u.filename, ( index + 1 ) );

	/* Open file */
	fd = linux_open ( filename, O_RDONLY );
	if ( ( fd < 0 ) && ( index == 0 ) ) {
		filename[ sizeof ( prefix ) - 1 /* NUL */ +
			  4 /* signature */ ] = '\0';
		fd = linux_open ( filename, O_RDONLY );
	}
	if ( fd < 0 ) {
		DBGC ( &linux_acpi_tables, "ACPI could not open %s: %s\n",
		       filename, linux_strerror ( linux_errno ) );
		goto err_open;
	}

	/* Read header */
	rlen = linux_read ( fd, &header, sizeof ( header ) );
	if ( rlen < 0 ) {
		DBGC ( &linux_acpi_tables, "ACPI could not read %s header: "
		       "%s\n", filename, linux_strerror ( linux_errno ) );
		goto err_header;
	}
	if ( ( ( size_t ) rlen ) != sizeof ( header ) ) {
		DBGC ( &linux_acpi_tables, "ACPI underlength %s header\n",
		       filename );
		goto err_header_len;
	}

	/* Parse header */
	len = le32_to_cpu ( header.length );
	if ( len < sizeof ( header ) ) {
		DBGC ( &linux_acpi_tables, "ACPI malformed %s header\n",
		       filename );
		goto err_malformed;
	}

	/* Allocate data */
	table->data = malloc ( len );
	if ( ! table->data )
		goto err_data;

	/* Read table */
	memcpy ( table->data, &header, sizeof ( header ) );
	data = ( table->data + sizeof ( header ) );
	len -= sizeof ( header );
	while ( len ) {
		rlen = linux_read ( fd, data, len );
		if ( rlen < 0 ) {
			DBGC ( &linux_acpi_tables, "ACPI could not read %s: "
			       "%s\n", filename,
			       linux_strerror ( linux_errno ) );
			goto err_body;
		}
		if ( rlen == 0 ) {
			DBGC ( &linux_acpi_tables, "ACPI underlength %s\n",
			       filename );
			goto err_body_len;
		}
		data += rlen;
		len -= rlen;
	}

	/* Close file */
	linux_close ( fd );

	/* Add to list of tables */
	list_add ( &table->list, &linux_acpi_tables );
	DBGC ( &linux_acpi_tables, "ACPI cached %s\n", filename );

	return virt_to_user ( table->data );

 err_body_len:
 err_body:
	free ( table->data );
 err_data:
 err_malformed:
 err_header_len:
 err_header:
	linux_close ( fd );
 err_open:
	free ( table );
 err_alloc:
	return UNULL;
}

PROVIDE_ACPI ( linux, acpi_find, linux_acpi_find );
