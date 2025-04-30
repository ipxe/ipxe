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
 * ACPI Root System Description Pointer
 *
 */

#include <stdint.h>
#include <string.h>
#include <realmode.h>
#include <bios.h>
#include <ipxe/acpi.h>
#include <ipxe/rsdp.h>

/** EBDA RSDP maximum segment */
#define RSDP_EBDA_END_SEG 0xa000

/** Fixed BIOS area RSDP start address */
#define RSDP_BIOS_START 0xe0000

/** Fixed BIOS area RSDP length */
#define RSDP_BIOS_LEN 0x20000

/** Stride at which to search for RSDP */
#define RSDP_STRIDE 16

/**
 * Locate ACPI root system description table within a memory range
 *
 * @v start		Start address to search
 * @v len		Length to search
 * @ret rsdt		ACPI root system description table, or NULL
 */
static const struct acpi_rsdt * rsdp_find_rsdt_range ( const void *start,
						       size_t len ) {
	static const char signature[8] = RSDP_SIGNATURE;
	const struct acpi_rsdp *rsdp;
	const struct acpi_rsdt *rsdt;
	size_t offset;
	uint8_t sum;
	unsigned int i;

	/* Search for RSDP */
	for ( offset = 0 ; ( ( offset + sizeof ( *rsdp ) ) < len ) ;
	      offset += RSDP_STRIDE ) {

		/* Check signature and checksum */
		rsdp = ( start + offset );
		if ( memcmp ( rsdp->signature, signature,
			      sizeof ( signature ) ) != 0 )
			continue;
		for ( sum = 0, i = 0 ; i < sizeof ( *rsdp ) ; i++ )
			sum += *( ( ( uint8_t * ) rsdp ) + i );
		if ( sum != 0 )
			continue;

		/* Extract RSDT */
		rsdt = phys_to_virt ( le32_to_cpu ( rsdp->rsdt ) );
		DBGC ( rsdt, "RSDT %#08lx found via RSDP %#08lx\n",
		       virt_to_phys ( rsdt ),
		       ( virt_to_phys ( start ) + offset ) );
		return rsdt;
	}

	return NULL;
}

/**
 * Locate ACPI root system description table
 *
 * @ret rsdt		ACPI root system description table, or NULL
 */
static const struct acpi_rsdt * rsdp_find_rsdt ( void ) {
	static const struct acpi_rsdt *rsdt;
	const void *ebda;
	uint16_t ebda_seg;
	size_t ebda_len;

	/* Return existing RSDT if already found */
	if ( rsdt )
		return rsdt;

	/* Search EBDA */
	get_real ( ebda_seg, BDA_SEG, BDA_EBDA );
	if ( ebda_seg < RSDP_EBDA_END_SEG ) {
	     ebda = real_to_virt ( ebda_seg, 0 );
	     ebda_len = ( ( RSDP_EBDA_END_SEG - ebda_seg ) * 16 );
	     rsdt = rsdp_find_rsdt_range ( ebda, ebda_len );
	     if ( rsdt )
		     return rsdt;
	}

	/* Search fixed BIOS area */
	rsdt = rsdp_find_rsdt_range ( phys_to_virt ( RSDP_BIOS_START ),
				      RSDP_BIOS_LEN );
	if ( rsdt )
		return rsdt;

	return NULL;
}

PROVIDE_ACPI ( rsdp, acpi_find_rsdt, rsdp_find_rsdt );
PROVIDE_ACPI_INLINE ( rsdp, acpi_find );
