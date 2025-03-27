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

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_table.h>

/** @file
 *
 * EFI configuration tables
 *
 */

/**
 * Look up EFI configuration table
 *
 * @v guid		Configuration table GUID
 * @ret table		Configuration table, or NULL
 */
void * efi_find_table ( EFI_GUID *guid ) {
	void *table;
	unsigned int i;

	/* Scan for installed table */
	for ( i = 0 ; i < efi_systab->NumberOfTableEntries ; i++ ) {
		if ( memcmp ( &efi_systab->ConfigurationTable[i].VendorGuid,
			      guid, sizeof ( *guid ) ) == 0 ) {
			table = efi_systab->ConfigurationTable[i].VendorTable;
			DBGC ( guid, "EFITAB %s is at %p\n",
			       efi_guid_ntoa ( guid ), table );
			return table;
		}
	}

	return NULL;
}

/**
 * Install EFI configuration table
 *
 * @v table		Configuration table type
 * @v data		Configuration table data, or NULL to uninstall
 * @v backup		Table backup, or NULL to not back up old table
 * @ret rc		Return status code
 */
int efi_install_table ( struct efi_table *table, const void *data,
			void **backup ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_GUID *guid = table->guid;
	void *copy;
	void *new;
	void *old;
	size_t old_len;
	size_t new_len;
	EFI_STATUS efirc;
	int rc;

	/* Get currently installed table, if any */
	old = efi_find_table ( guid );
	old_len = ( old ? table->len ( old ) : 0 );

	/* Create backup copy, if applicable */
	if ( old_len && backup ) {
		if ( ( efirc = bs->AllocatePool ( EfiBootServicesData, old_len,
						  &copy ) ) != 0 ) {
			rc = -EEFI ( efirc );
			goto err_backup;
		}
		memcpy ( copy, old, old_len );
		DBGC ( table, "EFITAB %s %p+%#zx backed up\n",
		       efi_guid_ntoa ( guid ), old, old_len );
	} else {
		copy = NULL;
	}

	/* Create installable runtime services data copy, if applicable */
	new_len = ( data ? table->len ( data ) : 0 );
	if ( new_len ) {
		if ( ( efirc = bs->AllocatePool ( EfiRuntimeServicesData,
						  new_len, &new ) ) != 0 ) {
			rc = -EEFI ( efirc );
			goto err_allocate;
		}
		memcpy ( new, data, new_len );
	} else {
		new = NULL;
	}

	/* (Un)install configuration table, if applicable */
	if ( new || old ) {
		if ( ( efirc = bs->InstallConfigurationTable ( guid,
							       new ) ) != 0 ) {
			rc = -EEFI ( efirc );
			DBGC ( table, "EFITAB %s could not install: %s\n",
			       efi_guid_ntoa ( guid ), strerror ( rc ) );
			goto err_install;
		}
		if ( old ) {
			DBGC ( table, "EFITAB %s %p+%#zx uninstalled\n",
			       efi_guid_ntoa ( guid ), old, old_len );
		}
		if ( new ) {
			DBGC ( table, "EFITAB %s %p+%#zx installed\n",
			       efi_guid_ntoa ( guid ), new, new_len );
		}
	}

	/* Record backup copy, if applicable */
	if ( backup ) {
		if ( *backup )
			bs->FreePool ( *backup );
		*backup = copy;
	}

	/* Sanity check */
	assert ( efi_find_table ( guid ) == new );

	return 0;

 err_install:
	if ( new )
		bs->FreePool ( new );
 err_allocate:
	if ( copy )
		bs->FreePool ( copy );
 err_backup:
	return rc;
}

/**
 * Uninstall EFI configuration table
 *
 * @v table		Configuration table type
 * @v backup		Table backup (or NULL to not restore old table)
 * @ret rc		Return status code
 */
int efi_uninstall_table ( struct efi_table *table, void **backup ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *old;
	int rc;

	/* Uninstall or reinstall as applicable */
	old = ( backup ? *backup : NULL );
	if ( ( rc = efi_install_table ( table, old, NULL ) ) != 0 )
		return rc;

	/* Free backup copy, if applicable */
	if ( backup && *backup ) {
		bs->FreePool ( *backup );
		*backup = NULL;
	}

	return 0;
}
