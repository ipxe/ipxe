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
 * Hardware threads (harts)
 *
 */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ipxe/fdt.h>
#include <ipxe/hart.h>

/** Boot hart ID */
unsigned long boot_hart;

/** Colour for debug messages */
#define colour &boot_hart

/**
 * Find boot hart node
 *
 * @v offset		Boot hart node offset
 * @ret rc		Return status code
 */
static int hart_node ( unsigned int *offset ) {
	char path[27 /* "/cpus/cpu@XXXXXXXXXXXXXXXX" + NUL */ ];
	int rc;

	/* Construct node path */
	snprintf ( path, sizeof ( path ), "/cpus/cpu@%lx", boot_hart );

	/* Find node */
	if ( ( rc = fdt_path ( &sysfdt, path, offset ) ) != 0 ) {
		DBGC ( colour, "HART could not find %s: %s\n",
		       path, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Check for supported extension
 *
 * @v ext		Extension name (including leading underscore)
 * @ret rc		Return status code
 */
int hart_supported ( const char *ext ) {
	unsigned int offset;
	const char *isa;
	const char *tmp;
	int rc;

	/* Find boot hart node */
	if ( ( rc = hart_node ( &offset ) ) != 0 )
		return rc;

	/* Get ISA description */
	isa = fdt_string ( &sysfdt, offset, "riscv,isa" );
	if ( ! isa ) {
		DBGC ( colour, "HART could not identify ISA\n" );
		return -ENOENT;
	}
	DBGC ( colour, "HART supports %s\n", isa );

	/* Check for presence of extension */
	tmp = isa;
	while ( ( tmp = strstr ( tmp, ext ) ) != NULL ) {
		tmp += strlen ( ext );
		if ( ( *tmp == '\0' ) || ( *tmp == '_' ) )
			return 0;
	}

	return -ENOTSUP;
}
