/*
 * Copyright (C) 2026 Bejean Mosher <bejean.mosher@intel.com>.
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

/** @file
 *
 * Universal Payload SMBIOS interface
 *
 */

#include <errno.h>
#include <ipxe/uaccess.h>
#include <ipxe/smbios.h>
#include <ipxe/upl.h>

#ifdef SMBIOS_UPL
#define SMBIOS_PREFIX_upl
#else
#define SMBIOS_PREFIX_upl __upl_
#endif

/** Length of region to search for an SMBIOS entry point */
#define UPL_SMBIOS_ENTRY_LEN 0x20

/**
 * Find SMBIOS
 *
 * @v smbios		SMBIOS entry point descriptor structure to fill in
 * @ret rc		Return status code
 */
static int upl_find_smbios ( struct smbios *smbios ) {
	const struct smbios3_entry *entry3;
	const struct smbios_entry *entry;
	const void *start;
	uint64_t addr;
	int rc;

	/* Locate SMBIOS reserved memory region */
	if ( ( rc = upl_fdt_reserved ( "smbios", 0, &addr ) ) != 0 )
		return rc;
	if ( ( ! addr ) || ( addr > ~( ( physaddr_t ) 0 ) ) ) {
		DBGC ( smbios, "UPL SMBIOS entry %#08llx is inaccessible\n",
		       ( ( unsigned long long ) addr ) );
		return -ENOENT;
	}
	start = phys_to_virt ( addr );

	/* Use 32-bit entry point if present */
	entry = find_smbios_entry ( start, UPL_SMBIOS_ENTRY_LEN );
	if ( entry ) {
		smbios->address = phys_to_virt ( entry->smbios_address );
		smbios->len = entry->smbios_len;
		smbios->count = entry->smbios_count;
		smbios->version = SMBIOS_VERSION ( entry->major,
						   entry->minor );
		return 0;
	}

	/* Otherwise, use 64-bit entry point if present and accessible */
	entry3 = find_smbios3_entry ( start, UPL_SMBIOS_ENTRY_LEN );
	if ( entry3 ) {
		if ( entry3->smbios_address > ~( ( physaddr_t ) 0 ) ) {
			DBGC ( smbios, "UPL SMBIOS3 at %08llx is "
			       "inaccessible\n",
			       ( ( unsigned long long )
				 entry3->smbios_address ) );
			return -ENOTSUP;
		}
		smbios->address = phys_to_virt ( entry3->smbios_address );
		smbios->len = entry3->smbios_len;
		smbios->count = 0;
		smbios->version = SMBIOS_VERSION ( entry3->major,
						   entry3->minor );
		return 0;
	}

	DBGC ( smbios, "UPL found no SMBIOS entry point at %#08lx\n",
	       virt_to_phys ( start ) );
	return -ENOENT;
}

PROVIDE_SMBIOS ( upl, find_smbios, upl_find_smbios );
