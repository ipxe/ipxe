/*
 * Copyright (C) 2019 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <string.h>
#include <byteswap.h>
#include <ipxe/fdt.h>
#include <ipxe/init.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_table.h>
#include <ipxe/efi/efi_fdt.h>
#include <ipxe/efi/Guid/Fdt.h>

/** @file
 *
 * EFI Flattened Device Tree
 *
 */

/** EFI Flattened Device Tree configuration table */
static struct fdt_header *efi_fdt;
EFI_USE_TABLE ( FDT_TABLE, &efi_fdt, 0 );

/**
 * Initialise EFI Flattened Device Tree
 *
 */
static void efi_fdt_init ( void ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_STATUS efirc;
	int rc;

	/* Do nothing if no configuration table is present */
	if ( ! efi_fdt ) {
		DBGC ( &efi_fdt, "EFIFDT has no configuration table\n" );
		return;
	}
	DBGC ( &efi_fdt, "EFIFDT configuration table at %p\n", efi_fdt );

	/* Parse as system device tree */
	if ( ( rc = fdt_parse ( &sysfdt, efi_fdt, -1UL ) ) != 0 ) {
		DBGC ( &efi_fdt, "EFIFDT could not parse: %s\n",
		       strerror ( rc ) );
		return;
	}

	/* Create copy, since table may be removed at any time */
	if ( ( efirc = bs->AllocatePool ( EfiBootServicesData, sysfdt.len,
					  &sysfdt.raw ) ) != 0 ) {
		DBGC ( &efi_fdt, "EFIFDT could not create copy\n" );
		sysfdt.len = 0;
		return;
	}
	memcpy ( sysfdt.raw, efi_fdt, sysfdt.len );
}

/** EFI Flattened Device Tree initialisation function */
struct init_fn efi_fdt_init_fn __init_fn ( INIT_EARLY ) = {
	.initialise = efi_fdt_init,
};

/**
 * Determine length of EFI Flattened Device Tree
 *
 * @v data		Configuration table data (presumed valid)
 * @ret len		Length of table
 */
static size_t efi_fdt_len ( const void *data ) {
	const struct fdt_header *hdr = data;

	return be32_to_cpu ( hdr->totalsize );
}

/** EFI Flattened Device Tree table type */
static struct efi_table efi_fdt_table = {
	.guid = &efi_fdt_table_guid,
	.len = efi_fdt_len,
};

/** EFI Flattened Device Tree table backup */
static void *efi_fdt_backup;

/** EFI Flattened Device Tree installed table */
static struct fdt_header *efi_fdt_installed;

/**
 * Install EFI Flattened Device Tree table
 *
 * @v cmdline		Command line, or NULL
 * @ret rc		Return status code
 */
int efi_fdt_install ( const char *cmdline ) {
	int rc;

	/* Create device tree */
	if ( ( rc = fdt_create ( &efi_fdt_installed, cmdline, 0, 0 ) ) != 0 ) {
		DBGC ( &efi_fdt, "EFI_FDT could not install: %s\n",
		       strerror ( rc ) );
		goto err_create;
	}

	/* Install table */
	if ( ( rc = efi_install_table ( &efi_fdt_table, efi_fdt_installed,
					&efi_fdt_backup ) ) != 0 ) {
		DBGC ( &efi_fdt, "EFIFDT could not install: %s\n",
		       strerror ( rc ) );
		goto err_install;
	}

	return 0;

	efi_uninstall_table ( &efi_fdt_table, &efi_fdt_backup );
 err_install:
	fdt_remove ( efi_fdt_installed );
 err_create:
	return rc;
}

/**
 * Uninstall EFI Flattened Device Tree table
 *
 * @ret rc		Return status code
 */
int efi_fdt_uninstall ( void ) {
	int rc;

	/* Uninstall table */
	if ( ( rc = efi_uninstall_table ( &efi_fdt_table,
					  &efi_fdt_backup ) ) != 0 ) {
		DBGC ( &efi_fdt, "EFIFDT could not %sinstall: %s\n",
		       ( efi_fdt_backup ? "re" : "un" ), strerror ( rc ) );
		/* Leak memory: there is nothing else we can do */
		return rc;
	}

	/* Remove table */
	fdt_remove ( efi_fdt_installed );

	return 0;
}
