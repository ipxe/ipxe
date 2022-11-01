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
 * iPXE ACPI API for EFI
 *
 */

#include <ipxe/acpi.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Guid/Acpi.h>
#include <ipxe/efi/efi_acpi.h>

/** ACPI configuration table */
static EFI_ACPI_1_0_ROOT_SYSTEM_DESCRIPTION_POINTER *rsdp;
EFI_USE_TABLE ( ACPI_10_TABLE, &rsdp, 0 );

/**
 * Locate ACPI root system description table
 *
 * @ret rsdt		ACPI root system description table, or UNULL
 */
static userptr_t efi_find_rsdt ( void ) {

	/* Locate RSDT via ACPI configuration table, if available */
	if ( rsdp )
		return phys_to_user ( rsdp->RsdtAddress );

	return UNULL;
}

PROVIDE_ACPI ( efi, acpi_find_rsdt, efi_find_rsdt );
