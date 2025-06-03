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
#include <string.h>
#include <errno.h>
#include <ipxe/linux_api.h>
#include <ipxe/linux_sysfs.h>
#include <ipxe/linux.h>
#include <ipxe/list.h>
#include <ipxe/init.h>
#include <ipxe/umalloc.h>
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
 * @ret table		Table, or NULL if not found
 */
static const struct acpi_header * linux_acpi_find ( uint32_t signature,
						    unsigned int index ) {
	struct linux_acpi_table *table;
	struct acpi_header *header;
	union {
		uint32_t signature;
		char filename[5];
	} u;
	static const char prefix[] = ACPI_SYSFS_PREFIX;
	char filename[ sizeof ( prefix ) - 1 /* NUL */ + 4 /* signature */
		       + 3 /* "999" */ + 1 /* NUL */ ];
	int len;
	int rc;

	/* Check for existing table */
	list_for_each_entry ( table, &linux_acpi_tables, list ) {
		if ( ( table->signature == signature ) &&
		     ( table->index == index ) )
			return table->data;
	}

	/* Allocate a new table */
	table = malloc ( sizeof ( *table ) );
	if ( ! table )
		goto err_alloc;
	table->signature = signature;
	table->index = index;

	/* Construct filename (including numeric suffix) */
	memset ( &u, 0, sizeof ( u ) );
	u.signature = le32_to_cpu ( signature );
	snprintf ( filename, sizeof ( filename ), "%s%s%d", prefix,
		   u.filename, ( index + 1 ) );

	/* Read file (with or without numeric suffix for index 0) */
	len = linux_sysfs_read ( filename, &table->data );
	if ( ( len < 0 ) && ( index == 0 ) ) {
		filename[ sizeof ( prefix ) - 1 /* NUL */ +
			  4 /* signature */ ] = '\0';
		len = linux_sysfs_read ( filename, &table->data );
	}
	if ( len < 0 ) {
		rc = len;
		DBGC ( &linux_acpi_tables, "ACPI could not read %s: %s\n",
		       filename, strerror ( rc ) );
		goto err_read;
	}
	header = table->data;
	if ( ( ( ( size_t ) len ) < sizeof ( *header ) ) ||
	     ( ( ( size_t ) len ) < le32_to_cpu ( header->length ) ) ) {
		rc = -ENOENT;
		DBGC ( &linux_acpi_tables, "ACPI underlength %s (%d bytes)\n",
		       filename, len );
		goto err_len;
	}

	/* Add to list of tables */
	list_add ( &table->list, &linux_acpi_tables );
	DBGC ( &linux_acpi_tables, "ACPI cached %s\n", filename );

	return table->data;

 err_len:
	ufree ( table->data );
 err_read:
	free ( table );
 err_alloc:
	return NULL;
}

/**
 * Free cached ACPI data
 *
 */
static void linux_acpi_shutdown ( int booting __unused ) {
	struct linux_acpi_table *table;
	struct linux_acpi_table *tmp;

	list_for_each_entry_safe ( table, tmp, &linux_acpi_tables, list ) {
		list_del ( &table->list );
		ufree ( table->data );
		free ( table );
	}
}

/** ACPI shutdown function */
struct startup_fn linux_acpi_startup_fn __startup_fn ( STARTUP_NORMAL ) = {
	.name = "linux_acpi",
	.shutdown = linux_acpi_shutdown,
};

PROVIDE_ACPI ( linux, acpi_find, linux_acpi_find );
