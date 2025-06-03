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
#include <realmode.h>
#include <pnpbios.h>

/** @file
 *
 * System Management BIOS
 *
 */

/**
 * Find SMBIOS
 *
 * @v smbios		SMBIOS entry point descriptor structure to fill in
 * @ret rc		Return status code
 */
static int bios_find_smbios2 ( struct smbios *smbios ) {
	const struct smbios_entry *entry;

	/* Scan through BIOS segment to find SMBIOS 32-bit entry point */
	entry = find_smbios_entry ( real_to_virt ( BIOS_SEG, 0 ), 0x10000 );
	if ( ! entry )
		return -ENOENT;

	/* Fill in entry point descriptor structure */
	smbios->address = phys_to_virt ( entry->smbios_address );
	smbios->len = entry->smbios_len;
	smbios->count = entry->smbios_count;
	smbios->version = SMBIOS_VERSION ( entry->major, entry->minor );

	return 0;
}

/**
 * Find SMBIOS
 *
 * @v smbios		SMBIOS entry point descriptor structure to fill in
 * @ret rc		Return status code
 */
static int bios_find_smbios3 ( struct smbios *smbios ) {
	const struct smbios3_entry *entry;

	/* Scan through BIOS segment to find SMBIOS 64-bit entry point */
	entry = find_smbios3_entry ( real_to_virt ( BIOS_SEG, 0 ), 0x10000 );
	if ( ! entry )
		return -ENOENT;

	/* Check that address is accessible */
	if ( entry->smbios_address > ~( ( physaddr_t ) 0 ) ) {
		DBG ( "SMBIOS3 at %08llx is inaccessible\n",
		      ( ( unsigned long long ) entry->smbios_address ) );
		return -ENOTSUP;
	}

	/* Fill in entry point descriptor structure */
	smbios->address = phys_to_virt ( entry->smbios_address );
	smbios->len = entry->smbios_len;
	smbios->count = 0;
	smbios->version = SMBIOS_VERSION ( entry->major, entry->minor );

	return 0;
}

/**
 * Find SMBIOS
 *
 * @v smbios		SMBIOS entry point descriptor structure to fill in
 * @ret rc		Return status code
 */
static int bios_find_smbios ( struct smbios *smbios ) {
	int rc;

	/* Use 32-bit table if present */
	if ( ( rc = bios_find_smbios2 ( smbios ) ) == 0 )
		return 0;

	/* Otherwise, use 64-bit table if present and accessible */
	if ( ( rc = bios_find_smbios3 ( smbios ) ) == 0 )
		return 0;

	return rc;
}

PROVIDE_SMBIOS ( pcbios, find_smbios, bios_find_smbios );
