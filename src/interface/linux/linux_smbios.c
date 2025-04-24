/*
 * Copyright (C) 2013 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <string.h>
#include <errno.h>
#include <ipxe/linux_api.h>
#include <ipxe/linux_sysfs.h>
#include <ipxe/linux.h>
#include <ipxe/umalloc.h>
#include <ipxe/init.h>
#include <ipxe/smbios.h>

/** SMBIOS entry point filename */
static const char smbios_entry_filename[] =
	"/sys/firmware/dmi/tables/smbios_entry_point";

/** SMBIOS filename */
static const char smbios_filename[] = "/sys/firmware/dmi/tables/DMI";

/** Cache SMBIOS data */
static void *smbios_data;

/**
 * Find SMBIOS
 *
 * @v smbios		SMBIOS entry point descriptor structure to fill in
 * @ret rc		Return status code
 */
static int linux_find_smbios ( struct smbios *smbios ) {
	struct smbios3_entry *smbios3_entry;
	struct smbios_entry *smbios_entry;
	void *entry;
	void *data;
	int len;
	int rc;

	/* Read entry point file */
	len = linux_sysfs_read ( smbios_entry_filename, &entry );
	if ( len < 0 ) {
		rc = len;
		DBGC ( smbios, "SMBIOS could not read %s: %s\n",
		       smbios_entry_filename, strerror ( rc ) );
		goto err_entry;
	}
	data = entry;
	smbios3_entry = data;
	smbios_entry = data;
	if ( ( len >= ( ( int ) sizeof ( *smbios3_entry ) ) ) &&
	     ( smbios3_entry->signature == SMBIOS3_SIGNATURE ) ) {
		smbios->version = SMBIOS_VERSION ( smbios3_entry->major,
						   smbios3_entry->minor );
	} else if ( ( len >= ( ( int ) sizeof ( *smbios_entry ) ) ) &&
		    ( smbios_entry->signature == SMBIOS_SIGNATURE ) ) {
		smbios->version = SMBIOS_VERSION ( smbios_entry->major,
						   smbios_entry->minor );
	} else {
		DBGC ( smbios, "SMBIOS invalid entry point %s:\n",
		       smbios_entry_filename );
		DBGC_HDA ( smbios, 0, data, len );
		rc = -EINVAL;
		goto err_version;
	}

	/* Read SMBIOS file */
	len = linux_sysfs_read ( smbios_filename, &smbios_data );
	if ( len < 0 ) {
		rc = len;
		DBGC ( smbios, "SMBIOS could not read %s: %s\n",
		       smbios_filename, strerror ( rc ) );
		goto err_read;
	}

	/* Populate SMBIOS descriptor */
	smbios->address = smbios_data;
	smbios->len = len;
	smbios->count = 0;

	/* Free entry point */
	ufree ( entry );

	return 0;

	ufree ( smbios_data );
	smbios_data = NULL;
 err_read:
 err_version:
	ufree ( entry );
 err_entry:
	return rc;
}

/**
 * Free cached SMBIOS data
 *
 */
static void linux_smbios_shutdown ( int booting __unused ) {

	/* Clear SMBIOS data pointer */
	smbios_clear();

	/* Free SMBIOS data */
	ufree ( smbios_data );
	smbios_data = NULL;
}

/** SMBIOS shutdown function */
struct startup_fn linux_smbios_startup_fn __startup_fn ( STARTUP_NORMAL ) = {
	.name = "linux_smbios",
	.shutdown = linux_smbios_shutdown,
};

PROVIDE_SMBIOS ( linux, find_smbios, linux_find_smbios );
