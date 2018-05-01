/*
 * Copyright (C) 2017 Michael Brown <mbrown@fensystems.co.uk>.
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

/**
 * @file
 *
 * ACPI settings
 *
 */

#include <string.h>
#include <errno.h>
#include <ipxe/init.h>
#include <ipxe/settings.h>
#include <ipxe/acpi.h>

/** ACPI settings scope */
static const struct settings_scope acpi_settings_scope;

/**
 * Check applicability of ACPI setting
 *
 * @v settings		Settings block
 * @v setting		Setting
 * @ret applies		Setting applies within this settings block
 */
static int acpi_settings_applies ( struct settings *settings __unused,
				   const struct setting *setting ) {

	return ( setting->scope == &acpi_settings_scope );
}

/**
 * Fetch value of ACPI setting
 *
 * @v settings		Settings block
 * @v setting		Setting to fetch
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int acpi_settings_fetch ( struct settings *settings,
				 struct setting *setting,
				 void *data, size_t len ) {
	struct acpi_header acpi;
	uint32_t tag_high;
	uint32_t tag_low;
	uint32_t tag_signature;
	unsigned int tag_index;
	size_t tag_offset;
	size_t tag_len;
	userptr_t table;
	size_t offset;
	size_t max_len;
	int delta;
	unsigned int i;

	/* Parse settings tag */
	tag_high = ( setting->tag >> 32 );
	tag_low = setting->tag;
	tag_signature = bswap_32 ( tag_high );
	tag_index = ( ( tag_low >> 24 ) & 0xff );
	tag_offset = ( ( tag_low >> 8 ) & 0xffff );
	tag_len = ( ( tag_low >> 0 ) & 0xff );
	DBGC ( settings, "ACPI %s.%d offset %#zx length %#zx\n",
	       acpi_name ( tag_signature ), tag_index, tag_offset, tag_len );

	/* Locate ACPI table */
	table = acpi_find ( tag_signature, tag_index );
	if ( ! table )
		return -ENOENT;

	/* Read table header */
	copy_from_user ( &acpi, table, 0, sizeof ( acpi ) );

	/* Calculate starting offset and maximum available length */
	max_len = le32_to_cpu ( acpi.length );
	if ( tag_offset > max_len )
		return -ENOENT;
	offset = tag_offset;
	max_len -= offset;

	/* Restrict to requested length, if specified */
	if ( tag_len && ( tag_len < max_len ) )
		max_len = tag_len;

	/* Invert endianness for numeric settings */
	if ( setting->type && setting->type->numerate ) {
		offset += ( max_len - 1 );
		delta = -1;
	} else {
		delta = +1;
	}

	/* Read data */
	for ( i = 0 ; ( ( i < max_len ) && ( i < len ) ) ; i++ ) {
		copy_from_user ( data, table, offset, 1 );
		data++;
		offset += delta;
	}

	/* Set type if not already specified */
	if ( ! setting->type )
		setting->type = &setting_type_hexraw;

	return max_len;
}

/** ACPI settings operations */
static struct settings_operations acpi_settings_operations = {
	.applies = acpi_settings_applies,
	.fetch = acpi_settings_fetch,
};

/** ACPI settings */
static struct settings acpi_settings = {
	.refcnt = NULL,
	.siblings = LIST_HEAD_INIT ( acpi_settings.siblings ),
	.children = LIST_HEAD_INIT ( acpi_settings.children ),
	.op = &acpi_settings_operations,
	.default_scope = &acpi_settings_scope,
};

/** Initialise ACPI settings */
static void acpi_settings_init ( void ) {
	int rc;

	if ( ( rc = register_settings ( &acpi_settings, NULL,
					"acpi" ) ) != 0 ) {
		DBG ( "ACPI could not register settings: %s\n",
		      strerror ( rc ) );
		return;
	}
}

/** ACPI settings initialiser */
struct init_fn acpi_settings_init_fn __init_fn ( INIT_NORMAL ) = {
	.initialise = acpi_settings_init,
};
