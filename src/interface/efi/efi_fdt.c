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
#include <ipxe/fdt.h>
#include <ipxe/efi/efi.h>
#include <ipxe/init.h>

/** @file
 *
 * EFI Flattened Device Tree
 *
 */

#define DEVICE_TREE_TABLE_GUID						\
	{ 0xb1b621d5, 0xf19c, 0x41a5,					\
	  { 0x83, 0x0b, 0xd9, 0x15, 0x2c, 0x69, 0xaa, 0xe0 } }

/** EFI Flattened Device Tree configuration table */
static struct fdt_header *efi_fdt;
EFI_USE_TABLE ( DEVICE_TREE_TABLE, &efi_fdt, 0 );

/**
 * Initialise EFI Flattened Device Tree
 *
 */
static void efi_fdt_init ( void ) {
	int rc;

	/* Do nothing if no configuration table is present */
	if ( ! efi_fdt ) {
		DBGC ( &efi_fdt, "EFIFDT has no configuration table\n" );
		return;
	}
	DBGC ( &efi_fdt, "EFIFDT configuration table at %p\n", efi_fdt );

	/* Register device tree */
	if ( ( rc = register_fdt ( efi_fdt ) ) != 0 ) {
		DBGC ( &efi_fdt, "EFIFDT could not register: %s\n",
		       strerror ( rc ) );
		return;
	}
}

/** EFI Flattened Device Tree initialisation function */
struct init_fn efi_fdt_init_fn __init_fn ( INIT_EARLY ) = {
	.initialise = efi_fdt_init,
};
