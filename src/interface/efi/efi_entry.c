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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>
#include <gpxe/efi/efi.h>
#include <gpxe/uuid.h>

/** Image handle passed to entry point */
EFI_HANDLE efi_image_handle;

/** System table passed to entry point */
EFI_SYSTEM_TABLE *efi_systab;

/** Declared used EFI protocols */
static struct efi_protocol efi_protocols[0] \
	__table_start ( struct efi_protocol, efi_protocols );
static struct efi_protocol efi_protocols_end[0] \
	__table_end ( struct efi_protocol, efi_protocols );

/**
 * EFI entry point
 *
 * @v image_handle	Image handle
 * @v systab		System table
 * @ret efirc		EFI return status code
 */
EFI_STATUS EFIAPI efi_entry ( EFI_HANDLE image_handle,
			      EFI_SYSTEM_TABLE *systab ) {
	EFI_BOOT_SERVICES *bs;
	struct efi_protocol *prot;
	EFI_STATUS efirc;

	/* Store image handle and system table pointer for future use */
	efi_image_handle = image_handle;
	efi_systab = systab;

	/* Sanity checks */
	if ( ! systab )
		return EFI_NOT_AVAILABLE_YET;
	if ( ! systab->ConOut )
		return EFI_NOT_AVAILABLE_YET;
	if ( ! systab->BootServices ) {
		DBGC ( systab, "EFI provided no BootServices entry point\n" );
		return EFI_NOT_AVAILABLE_YET;
	}
	if ( ! systab->RuntimeServices ) {
		DBGC ( systab, "EFI provided no RuntimeServices entry "
		       "point\n" );
		return EFI_NOT_AVAILABLE_YET;
	}
	DBGC ( systab, "EFI handle %p systab %p\n", image_handle, systab );

	/* Look up required protocols */
	bs = systab->BootServices;
	for ( prot = efi_protocols ; prot < efi_protocols_end ; prot++ ) {
		if ( ( efirc = bs->LocateProtocol ( &prot->u.guid, NULL,
						    prot->protocol ) ) != 0 ) {
			DBGC ( systab, "EFI does not provide protocol %s\n",
			       uuid_ntoa ( &prot->u.uuid ) );
			return efirc;
		}
		DBGC ( systab, "EFI protocol %s is at %p\n",
		       uuid_ntoa ( &prot->u.uuid ), *(prot->protocol) );
	}

	/* Call to main() */
	return RC_TO_EFIRC ( main () );
}
