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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <gpxe/acpi.h>

/** @file
 *
 * ACPI support functions
 *
 */

/**
 * Fix up ACPI table checksum
 *
 * @v acpi		ACPI table header
 */
void acpi_fix_checksum ( struct acpi_description_header *acpi ) {
	unsigned int i = 0;
	uint8_t sum = 0;

	for ( i = 0 ; i < acpi->length ; i++ ) {
		sum += *( ( ( uint8_t * ) acpi ) + i );
	}
	acpi->checksum -= sum;
}
