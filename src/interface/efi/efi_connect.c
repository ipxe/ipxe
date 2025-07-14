/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
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
 * EFI driver connection and disconnection
 *
 */

#include <errno.h>
#include <string.h>
#include <ipxe/efi/efi.h>

/* Disambiguate the various error causes */
#define EINFO_EEFI_CONNECT						\
	__einfo_uniqify ( EINFO_EPLATFORM, 0x01,			\
			  "Could not connect controllers" )
#define EINFO_EEFI_CONNECT_PROHIBITED					\
	__einfo_platformify ( EINFO_EEFI_CONNECT,			\
			      EFI_SECURITY_VIOLATION,			\
			      "Connecting controllers prohibited by "	\
			      "security policy" )
#define EEFI_CONNECT_PROHIBITED						\
	__einfo_error ( EINFO_EEFI_CONNECT_PROHIBITED )
#define EEFI_CONNECT( efirc ) EPLATFORM ( EINFO_EEFI_CONNECT, efirc,	\
					  EEFI_CONNECT_PROHIBITED )

/**
 * Connect UEFI driver(s)
 *
 * @v device		EFI device handle
 * @v driver		EFI driver handle, or NULL
 * @ret rc		Return status code
 */
int efi_connect ( EFI_HANDLE device, EFI_HANDLE driver ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_HANDLE driverlist[2] = { driver, NULL };
	EFI_HANDLE *drivers = ( driver ? driverlist : NULL );
	EFI_STATUS efirc;
	int rc;

	/* Attempt connection at external TPL */
	DBGC ( device, "EFI %s connecting ", efi_handle_name ( device ) );
	DBGC ( device, "%s driver at %s TPL\n",
	       ( driver ? efi_handle_name ( driver ) : "any" ),
	       efi_tpl_name ( efi_external_tpl ) );
	bs->RestoreTPL ( efi_external_tpl );
	efirc = bs->ConnectController ( device, drivers, NULL, TRUE );
	bs->RaiseTPL ( efi_internal_tpl );
	if ( efirc != 0 ) {
		rc = -EEFI_CONNECT ( efirc );
		DBGC ( device, "EFI %s could not connect: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Disconnect UEFI driver(s)
 *
 * @v device		EFI device handle
 * @v driver		EFI driver handle, or NULL
 * @ret rc		Return status code
 */
int efi_disconnect ( EFI_HANDLE device, EFI_HANDLE driver ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_STATUS efirc;
	int rc;

	/* Attempt disconnection at external TPL */
	DBGC ( device, "EFI %s disconnecting ", efi_handle_name ( device ) );
	DBGC ( device, "%s driver at %s TPL\n",
	       ( driver ? efi_handle_name ( driver ) : "any" ),
	       efi_tpl_name ( efi_external_tpl ) );
	bs->RestoreTPL ( efi_external_tpl );
	efirc = bs->DisconnectController ( device, driver, NULL );
	bs->RaiseTPL ( efi_internal_tpl );
	if ( ( efirc != 0 ) && ( efirc != EFI_NOT_FOUND ) ) {
		rc = -EEFI ( efirc );
		DBGC ( device, "EFI %s could not disconnect: %s\n",
		       efi_handle_name ( device ), strerror ( rc ) );
		return rc;
	}

	return 0;
}
