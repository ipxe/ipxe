/*
 * Copyright (C) 2008 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <errno.h>
#include <ipxe/uaccess.h>
#include <ipxe/smbios.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Guid/SmBios.h>

/** @file
 *
 * iPXE SMBIOS API for EFI
 *
 */

/** SMBIOS configuration table */
static struct smbios_entry *smbios_entry;
EFI_USE_TABLE ( SMBIOS_TABLE, &smbios_entry, 0 );

/** SMBIOS configuration table */
static struct smbios3_entry *smbios3_entry;
EFI_USE_TABLE ( SMBIOS3_TABLE, &smbios3_entry, 0 );

/**
 * Find SMBIOS
 *
 * @v smbios		SMBIOS entry point descriptor structure to fill in
 * @ret rc		Return status code
 */
static int efi_find_smbios ( struct smbios *smbios ) {

	/* Use 64-bit table if present */
	if ( smbios3_entry && ( smbios3_entry->signature == SMBIOS3_SIGNATURE ) ) {
		smbios->address = phys_to_virt ( smbios3_entry->smbios_address );
		smbios->len = smbios3_entry->smbios_len;
		smbios->count = 0;
		smbios->version =
			SMBIOS_VERSION ( smbios3_entry->major, smbios3_entry->minor );
		DBG ( "Found 64-bit SMBIOS v%d.%d entry point at %p (%lx+%zx)\n",
		      smbios3_entry->major, smbios3_entry->minor, smbios3_entry,
		      virt_to_phys ( smbios->address ), smbios->len );
		return 0;
	}

	/* Otherwise, use 32-bit table if present */
	if ( smbios_entry && ( smbios_entry->signature == SMBIOS_SIGNATURE ) ) {
		smbios->address = phys_to_virt ( smbios_entry->smbios_address );
		smbios->len = smbios_entry->smbios_len;
		smbios->count = smbios_entry->smbios_count;
		smbios->version =
			SMBIOS_VERSION ( smbios_entry->major, smbios_entry->minor );
		DBG ( "Found 32-bit SMBIOS v%d.%d entry point at %p (%lx+%zx)\n",
		      smbios_entry->major, smbios_entry->minor, smbios_entry,
		      virt_to_phys ( smbios->address ), smbios->len );
		return 0;
	}

	DBG ( "No SMBIOS table provided\n" );
	return -ENODEV;
}

PROVIDE_SMBIOS ( efi, find_smbios, efi_find_smbios );
