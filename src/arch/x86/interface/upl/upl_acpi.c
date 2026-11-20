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
 * Universal Payload ACPI interface
 *
 */

#include <string.h>
#include <byteswap.h>
#include <ipxe/uaccess.h>
#include <ipxe/acpi.h>
#include <ipxe/upl.h>

/**
 * Get ACPI RSDP physical address
 *
 * @ret rsdp		RSDP physical address, or zero if unavailable
 */
physaddr_t upl_acpi_rsdp ( void ) {
	static const char signature[8] = RSDP_SIGNATURE;
	const struct acpi_rsdp *rsdp;
	unsigned int index;
	uint64_t addr;
	uint8_t sum;
	unsigned int i;

	/* Find the ACPI reserved memory region that starts with the RSDP */
	for ( index = 0 ; upl_fdt_reserved ( "acpi", index, &addr ) == 0 ;
	      index++ ) {

		/* Check accessibility */
		if ( ( ! addr ) || ( addr > ~( ( physaddr_t ) 0 ) ) ) {
			DBGC ( &signature, "UPL ACPI region %#08llx is "
			       "inaccessible\n",
			       ( ( unsigned long long ) addr ) );
			continue;
		}
		rsdp = phys_to_virt ( addr );

		/* Validate RSDP */
		if ( memcmp ( rsdp->signature, signature,
			      sizeof ( signature ) ) != 0 )
			continue;
		for ( sum = 0, i = 0 ; i < sizeof ( *rsdp ) ; i++ )
			sum += *( ( ( const uint8_t * ) rsdp ) + i );
		if ( sum != 0 ) {
			DBGC ( rsdp, "UPL ACPI RSDP %#08lx has bad checksum\n",
			       virt_to_phys ( rsdp ) );
			continue;
		}

		return virt_to_phys ( rsdp );
	}

	DBGC ( &signature, "UPL found no ACPI RSDP\n" );
	return 0;
}

/**
 * Locate ACPI root system description table
 *
 * @ret rsdt		ACPI root system description table, or NULL
 */
static const struct acpi_rsdt * upl_find_rsdt ( void ) {
	const struct acpi_rsdp *rsdp;
	physaddr_t phys;

	/* Locate RSDP */
	phys = upl_acpi_rsdp();
	if ( ! phys )
		return NULL;
	rsdp = phys_to_virt ( phys );

	/* Extract RSDT */
	if ( ! rsdp->rsdt )
		return NULL;
	return phys_to_virt ( le32_to_cpu ( rsdp->rsdt ) );
}

PROVIDE_ACPI ( upl, acpi_find_rsdt, upl_find_rsdt );
PROVIDE_ACPI_INLINE ( upl, acpi_find );
