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
#include <ipxe/efi/Guid/GlobalVariable.h>

/** An EFI variable settings block */
struct efivar_settings {
	/** Settings block */
	struct settings settings;
	/** Vendor GUID */
	EFI_GUID guid;
};

/** EFI settings scope */
static const struct settings_scope efivar_scope;

/**
 * Check applicability of EFI variable setting
 *
 * @v settings		Settings block
 * @v setting		Setting
 * @ret applies		Setting applies within this settings block
 */
static int efivar_applies ( struct settings *settings __unused,
			    const struct setting *setting ) {

	return ( setting->scope == &efivar_scope );
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
static int efivar_fetch ( struct settings *settings, struct setting *setting,
			  void *data, size_t len ) {
	EFI_RUNTIME_SERVICES *rs = efi_systab->RuntimeServices;
	struct efivar_settings *efivars =
		container_of ( settings, struct efivar_settings, settings );
	size_t name_len = strlen ( setting->name );
	CHAR16 wname[ name_len + 1 /* wNUL */ ];
	UINT32 attrs;
	UINTN size;
	void *buf;
	unsigned int i;
	EFI_STATUS efirc;
	int rc;

	/* Convert name to UCS-2 */
	for ( i = 0 ; i <= name_len ; i++ )
		wname[i] = setting->name[i];

	/* Get variable length */
	size = 0;
	efirc = rs->GetVariable ( wname, &efivars->guid, &attrs, &size, NULL );
	if ( ( efirc != 0 ) && ( efirc != EFI_BUFFER_TOO_SMALL ) ) {
		rc = -EEFI ( efirc );
		DBGC ( efivars, "EFIVAR %s:%s not present: %s\n",
		       efi_guid_ntoa ( &efivars->guid ), setting->name,
		       strerror ( rc ) );
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
	if ( ( efirc = rs->GetVariable ( wname, &efivars->guid, &attrs, &size,
					 buf ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( efivars, "EFIVAR %s:%s could not get %zd bytes: %s\n",
		       efi_guid_ntoa ( &efivars->guid ), setting->name,
		       ( ( size_t ) size ), strerror ( rc ) );
		goto err_get;
	}
	DBGC ( efivars, "EFIVAR %s:%s:\n", efi_guid_ntoa ( &efivars->guid ),
	       setting->name );
	DBGC_HDA ( efivars, 0, buf, size );

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
	return rc;
}

/** EFI variable settings operations */
static struct settings_operations efivar_operations = {
	.applies = efivar_applies,
	.fetch = efivar_fetch,
};

/** Well-known EFI variable settings blocks */
static struct efivar_settings efivar_settings[] = {
	{ .settings = { .name = "efi" }, .guid = EFI_GLOBAL_VARIABLE },
};

/**
 * Initialise EFI variable settings
 *
 */
static void efivar_init ( void ) {
	struct efivar_settings *efivars;
	const char *name;
	unsigned int i;
	int rc;

	/* Register all settings blocks */
	for ( i = 0 ; i < ( sizeof ( efivar_settings ) /
			    sizeof ( efivar_settings[0] ) ) ; i++ ) {

		/* Initialise settings block */
		efivars = &efivar_settings[i];
		settings_init ( &efivars->settings, &efivar_operations, NULL,
				&efivar_scope );

		/* Register settings block */
		name = efivars->settings.name;
		if ( ( rc = register_settings ( &efivars->settings, NULL,
						name ) ) != 0 ) {
			DBGC ( &efivar_settings, "EFIVAR could not register "
			       "%s: %s\n", name, strerror ( rc ) );
			/* Continue trying to register remaining blocks */
		}
	}
}

/** EFI variable settings initialiser */
struct init_fn efivar_init_fn __init_fn ( INIT_NORMAL ) = {
	.initialise = efivar_init,
};
