/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
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
 * EFI service binding
 *
 */

#include <string.h>
#include <errno.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_service.h>
#include <ipxe/efi/Protocol/ServiceBinding.h>

/**
 * Add service to child handle
 *
 * @v service		Service binding handle
 * @v binding		Service binding protocol GUID
 * @v handle		Handle on which to install child
 * @ret rc		Return status code
 */
int efi_service_add ( EFI_HANDLE service, EFI_GUID *binding,
		      EFI_HANDLE *handle ) {
	EFI_SERVICE_BINDING_PROTOCOL *sb;
	EFI_STATUS efirc;
	int rc;

	/* Open service binding protocol */
	if ( ( rc = efi_open ( service, binding, &sb ) ) != 0 ) {
		DBGC ( service, "EFISVC %s cannot open %s binding: %s\n",
		       efi_handle_name ( service ), efi_guid_ntoa ( binding ),
		       strerror ( rc ) );
		return rc;
	}

	/* Create child handle */
	if ( ( efirc = sb->CreateChild ( sb, handle ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( service, "EFISVC %s could not create %s child: %s\n",
		       efi_handle_name ( service ), efi_guid_ntoa ( binding ),
		       strerror ( rc ) );
		return rc;
	}

	DBGC ( service, "EFISVC %s created %s child ",
	       efi_handle_name ( service ), efi_guid_ntoa ( binding ) );
	DBGC ( service, "%s\n", efi_handle_name ( *handle ) );
	return 0;
}

/**
 * Remove service from child handle
 *
 * @v service		Service binding handle
 * @v binding		Service binding protocol GUID
 * @v handle		Child handle
 * @ret rc		Return status code
 */
int efi_service_del ( EFI_HANDLE service, EFI_GUID *binding,
		      EFI_HANDLE handle ) {
	EFI_SERVICE_BINDING_PROTOCOL *sb;
	EFI_STATUS efirc;
	int rc;

	DBGC ( service, "EFISVC %s removing %s child ",
	       efi_handle_name ( service ), efi_guid_ntoa ( binding ) );
	DBGC ( service, "%s\n", efi_handle_name ( handle ) );

	/* Open service binding protocol */
	if ( ( rc = efi_open ( service, binding, &sb ) ) != 0 ) {
		DBGC ( service, "EFISVC %s cannot open %s binding: %s\n",
		       efi_handle_name ( service ), efi_guid_ntoa ( binding ),
		       strerror ( rc ) );
		return rc;
	}

	/* Destroy child handle */
	if ( ( efirc = sb->DestroyChild ( sb, handle ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( service, "EFISVC %s could not destroy %s child ",
		       efi_handle_name ( service ), efi_guid_ntoa ( binding ) );
		DBGC ( service, "%s: %s\n",
		       efi_handle_name ( handle ), strerror ( rc ) );
		return rc;
	}

	return 0;
}
