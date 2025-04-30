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

/** @file
 *
 * EFI command line
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <ipxe/init.h>
#include <ipxe/image.h>
#include <ipxe/script.h>
#include <ipxe/uaccess.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_cmdline.h>

/** EFI command line (may not be wNUL-terminated */
const wchar_t *efi_cmdline;

/** Length of EFI command line (in bytes) */
size_t efi_cmdline_len;

/** Internal copy of the command line */
static char *efi_cmdline_copy;

/**
 * Free command line image
 *
 * @v refcnt		Reference count
 */
static void efi_cmdline_free ( struct refcnt *refcnt ) {
	struct image *image = container_of ( refcnt, struct image, refcnt );

	DBGC ( image, "CMDLINE freeing command line\n" );
	free_image ( refcnt );
	free ( efi_cmdline_copy );
}

/** Embedded script representing the command line */
static struct image efi_cmdline_image = {
	.refcnt = REF_INIT ( efi_cmdline_free ),
	.name = "<CMDLINE>",
	.flags = ( IMAGE_STATIC | IMAGE_STATIC_NAME ),
	.type = &script_image_type,
};

/** Colour for debug messages */
#define colour &efi_cmdline_image

/**
 * Initialise EFI command line
 *
 * @ret rc		Return status code
 */
static int efi_cmdline_init ( void ) {
	char *cmdline;
	size_t len;
	int rc;

	/* Do nothing if no command line was specified */
	if ( ! efi_cmdline_len ) {
		DBGC ( colour, "CMDLINE found no command line\n" );
		return 0;
	}

	/* Allocate ASCII copy of command line */
	len = ( ( efi_cmdline_len / sizeof ( efi_cmdline[0] ) ) + 1 /* NUL */ );
	efi_cmdline_copy = malloc ( len );
	if ( ! efi_cmdline_copy ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	cmdline = efi_cmdline_copy;
	snprintf ( cmdline, len, "%ls", efi_cmdline );
	DBGC ( colour, "CMDLINE found command line \"%s\"\n", cmdline );

	/* Mark command line as consumed */
	efi_cmdline_len = 0;

	/* Strip image name and surrounding whitespace */
	while ( isspace ( *cmdline ) )
		cmdline++;
	while ( *cmdline && ( ! isspace ( *cmdline ) ) )
		cmdline++;
	while ( isspace ( *cmdline ) )
		cmdline++;
	DBGC ( colour, "CMDLINE using command line \"%s\"\n", cmdline );

	/* Prepare and register image */
	efi_cmdline_image.data = cmdline;
	efi_cmdline_image.len = strlen ( cmdline );
	if ( efi_cmdline_image.len &&
	     ( ( rc = register_image ( &efi_cmdline_image ) ) != 0 ) ) {
		DBGC ( colour, "CMDLINE could not register command line: %s\n",
		       strerror ( rc ) );
		goto err_register_image;
	}

	/* Drop our reference to the image */
	image_put ( &efi_cmdline_image );

	return 0;

 err_register_image:
	image_put ( &efi_cmdline_image );
 err_alloc:
	return rc;
}

/**
 * EFI command line startup function
 *
 */
static void efi_cmdline_startup ( void ) {
	int rc;

	/* Initialise command line */
	if ( ( rc = efi_cmdline_init() ) != 0 ) {
		/* No way to report failure */
		return;
	}
}

/** Command line and initrd initialisation function */
struct startup_fn efi_cmdline_startup_fn __startup_fn ( STARTUP_NORMAL ) = {
	.name = "efi_cmdline",
	.startup = efi_cmdline_startup,
};
