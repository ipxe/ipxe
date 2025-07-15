/*
 * Copyright (C) 2023 Michael Brown <mbrown@fensystems.co.uk>.
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
 * EFI variable settings
 *
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ipxe/settings.h>
#include <ipxe/init.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_strings.h>

/** EFI variable settings scope */
static const struct settings_scope efivars_scope;

/** EFI variable settings */
static struct settings efivars;

/**
 * Check applicability of EFI variable setting
 *
 * @v settings		Settings block
 * @v setting		Setting
 * @ret applies		Setting applies within this settings block
 */
static int efivars_applies ( struct settings *settings __unused,
			     const struct setting *setting ) {

	return ( setting->scope == &efivars_scope );
}

/**
 * Find first matching EFI variable name
 *
 * @v wname		Name
 * @v guid		GUID to fill in
 * @ret rc		Return status code
 */
static int efivars_find ( const CHAR16 *wname, EFI_GUID *guid ) {
	EFI_RUNTIME_SERVICES *rs = efi_systab->RuntimeServices;
	size_t wname_len = ( ( wcslen ( wname ) + 1 ) * sizeof ( wname[0] ) );
	CHAR16 *buf;
	CHAR16 *tmp;
	UINTN size;
	EFI_STATUS efirc;
	int rc;

	/* Allocate single wNUL for first call to GetNextVariableName() */
	size = sizeof ( buf[0] );
	buf = zalloc ( size );
	if ( ! buf )
		return -ENOMEM;

	/* Iterate over all veriables */
	while ( 1 ) {

		/* Get next variable name */
		efirc = rs->GetNextVariableName ( &size, buf, guid );
		if ( efirc == EFI_BUFFER_TOO_SMALL ) {
			tmp = realloc ( buf, size );
			if ( ! tmp ) {
				rc = -ENOMEM;
				break;
			}
			buf = tmp;
			efirc = rs->GetNextVariableName ( &size, buf, guid );
		}
		if ( efirc == EFI_NOT_FOUND ) {
			rc = -ENOENT;
			break;
		}
		if ( efirc != 0 ) {
			rc = -EEFI ( efirc );
			DBGC ( &efivars, "EFIVARS %s:%ls could not fetch next "
			       "variable name: %s\n",
			       efi_guid_ntoa ( guid ), buf, strerror ( rc ) );
			break;
		}
		DBGC2 ( &efivars, "EFIVARS %s:%ls exists\n",
			efi_guid_ntoa ( guid ), buf );

		/* Check for matching variable name */
		if ( memcmp ( wname, buf, wname_len ) == 0 ) {
			rc = 0;
			break;
		}
	}

	/* Free temporary buffer */
	free ( buf );

	return rc;
}

/**
 * Fetch value of EFI variable setting
 *
 * @v settings		Settings block
 * @v setting		Setting to fetch
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int efivars_fetch ( struct settings *settings __unused,
			   struct setting *setting, void *data, size_t len ) {
	EFI_RUNTIME_SERVICES *rs = efi_systab->RuntimeServices;
	size_t name_len = strlen ( setting->name );
	CHAR16 wname[ name_len + 1 /* wNUL */ ];
	EFI_GUID guid;
	UINT32 attrs;
	UINTN size;
	void *buf;
	EFI_STATUS efirc;
	int rc;

	/* Convert name to UCS-2 */
	efi_snprintf ( wname, sizeof ( wname ), "%s", setting->name );

	/* Find variable GUID */
	if ( ( rc = efivars_find ( wname, &guid ) ) != 0 )
		goto err_find;

	/* Get variable length */
	size = 0;
	if ( ( efirc = rs->GetVariable ( wname, &guid, &attrs, &size,
					 NULL ) != EFI_BUFFER_TOO_SMALL ) ) {
		rc = -EEFI ( efirc );
		DBGC ( &efivars, "EFIVARS %s:%ls could not get size: %s\n",
		       efi_guid_ntoa ( &guid ), wname, strerror ( rc ) );
		goto err_len;
	}

	/* Allocate temporary buffer, since GetVariable() is not
	 * guaranteed to return partial data for an underlength
	 * buffer.
	 */
	buf = malloc ( size );
	if ( ! buf ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Get variable value */
	if ( ( efirc = rs->GetVariable ( wname, &guid, &attrs, &size,
					 buf ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( &efivars, "EFIVARS %s:%ls could not get %zd bytes: "
		       "%s\n", efi_guid_ntoa ( &guid ), wname,
		       ( ( size_t ) size ), strerror ( rc ) );
		goto err_get;
	}
	DBGC ( &efivars, "EFIVARS %s:%ls:\n", efi_guid_ntoa ( &guid ), wname );
	DBGC_HDA ( &efivars, 0, buf, size );

	/* Return setting value */
	if ( len > size )
		len = size;
	memcpy ( data, buf, len );
	if ( ! setting->type )
		setting->type = &setting_type_hex;

	/* Free temporary buffer */
	free ( buf );

	return size;

 err_get:
	free ( buf );
 err_alloc:
 err_len:
 err_find:
	return rc;
}

/** EFI variable settings operations */
static struct settings_operations efivars_operations = {
	.applies = efivars_applies,
	.fetch = efivars_fetch,
};

/** EFI variable settings */
static struct settings efivars = {
	.refcnt = NULL,
	.siblings = LIST_HEAD_INIT ( efivars.siblings ),
	.children = LIST_HEAD_INIT ( efivars.children ),
	.op = &efivars_operations,
	.default_scope = &efivars_scope,
};

/**
 * Initialise EFI variable settings
 *
 */
static void efivars_init ( void ) {
	int rc;

	/* Register settings block */
	if ( ( rc = register_settings ( &efivars, NULL, "efi" ) ) != 0 ) {
		DBGC ( &efivars, "EFIVARS could not register: %s\n",
		       strerror ( rc ) );
		return;
	}
}

/** EFI variable settings initialiser */
struct init_fn efivars_init_fn __init_fn ( INIT_NORMAL ) = {
	.name = "efivars",
	.initialise = efivars_init,
};
