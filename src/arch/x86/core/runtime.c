/*
 * Copyright (C) 2011 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Command line and initrd passed to iPXE at runtime
 *
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <ipxe/init.h>
#include <ipxe/image.h>
#include <ipxe/script.h>
#include <realmode.h>

/** Command line physical address
 *
 * This can be set by the prefix.
 */
uint32_t __bss16 ( cmdline_phys );
#define cmdline_phys __use_data16 ( cmdline_phys )

/** initrd physical address
 *
 * This can be set by the prefix.
 */
uint32_t __bss16 ( initrd_phys );
#define initrd_phys __use_data16 ( initrd_phys )

/** initrd length
 *
 * This can be set by the prefix.
 */
uint32_t __bss16 ( initrd_len );
#define initrd_len __use_data16 ( initrd_len )

/** Internal copy of the command line */
static char *cmdline_copy;

/** Free command line image */
static void cmdline_image_free ( struct refcnt *refcnt ) {
	struct image *image = container_of ( refcnt, struct image, refcnt );

	DBGC ( image, "RUNTIME freeing command line\n" );
	free_image ( refcnt );
	free ( cmdline_copy );
}

/** Embedded script representing the command line */
static struct image cmdline_image = {
	.refcnt = REF_INIT ( cmdline_image_free ),
	.name = "<CMDLINE>",
	.flags = ( IMAGE_STATIC | IMAGE_STATIC_NAME ),
	.type = &script_image_type,
};

/** Colour for debug messages */
#define colour &cmdline_image

/**
 * Strip unwanted cruft from command line
 *
 * @v cmdline		Command line
 * @v cruft		Initial substring of cruft to strip
 */
static void cmdline_strip ( char *cmdline, const char *cruft ) {
	char *strip;
	char *strip_end;

	/* Find unwanted cruft, if present */
	if ( ! ( strip = strstr ( cmdline, cruft ) ) )
		return;

	/* Strip unwanted cruft */
	strip_end = strchr ( strip, ' ' );
	if ( strip_end ) {
		*strip_end = '\0';
		DBGC ( colour, "RUNTIME stripping \"%s\"\n", strip );
		strcpy ( strip, ( strip_end + 1 ) );
	} else {
		DBGC ( colour, "RUNTIME stripping \"%s\"\n", strip );
		*strip = '\0';
	}
}

/**
 * Initialise command line
 *
 * @ret rc		Return status code
 */
static int cmdline_init ( void ) {
	char *cmdline;
	int rc;

	/* Do nothing if no command line was specified */
	if ( ! cmdline_phys ) {
		DBGC ( colour, "RUNTIME found no command line\n" );
		return 0;
	}

	/* Allocate and copy command line */
	cmdline_copy = strdup ( phys_to_virt ( cmdline_phys ) );
	if ( ! cmdline_copy ) {
		DBGC ( colour, "RUNTIME could not allocate command line\n" );
		rc = -ENOMEM;
		goto err_alloc_cmdline_copy;
	}
	cmdline = cmdline_copy;
	DBGC ( colour, "RUNTIME found command line \"%s\" at %08x\n",
	       cmdline, cmdline_phys );

	/* Mark command line as consumed */
	cmdline_phys = 0;

	/* Strip unwanted cruft from the command line */
	cmdline_strip ( cmdline, "BOOT_IMAGE=" );
	cmdline_strip ( cmdline, "initrd=" );
	while ( isspace ( *cmdline ) )
		cmdline++;
	DBGC ( colour, "RUNTIME using command line \"%s\"\n", cmdline );

	/* Prepare and register image */
	cmdline_image.data = cmdline;
	cmdline_image.len = strlen ( cmdline );
	if ( cmdline_image.len ) {
		if ( ( rc = register_image ( &cmdline_image ) ) != 0 ) {
			DBGC ( colour, "RUNTIME could not register command "
			       "line: %s\n", strerror ( rc ) );
			goto err_register_image;
		}
	}

	/* Drop our reference to the image */
	image_put ( &cmdline_image );

	return 0;

 err_register_image:
	image_put ( &cmdline_image );
 err_alloc_cmdline_copy:
	return rc;
}

/**
 * Initialise initrd
 *
 * @ret rc		Return status code
 */
static int initrd_init ( void ) {
	struct image *image;

	/* Do nothing if no initrd was specified */
	if ( ! initrd_phys ) {
		DBGC ( colour, "RUNTIME found no initrd\n" );
		return 0;
	}
	if ( ! initrd_len ) {
		DBGC ( colour, "RUNTIME found empty initrd\n" );
		return 0;
	}
	DBGC ( colour, "RUNTIME found initrd at [%x,%x)\n",
	       initrd_phys, ( initrd_phys + initrd_len ) );

	/* Create initrd image */
	image = image_memory ( "<INITRD>", phys_to_virt ( initrd_phys ),
			       initrd_len );
	if ( ! image ) {
		DBGC ( colour, "RUNTIME could not create initrd image\n" );
		return -ENOMEM;
	}

	/* Mark initrd as consumed */
	initrd_phys = 0;

	return 0;
}

/**
 * Initialise command line and initrd
 *
 */
static void runtime_init ( void ) {
	int rc;

	/* Initialise command line */
	if ( ( rc = cmdline_init() ) != 0 ) {
		/* No way to report failure */
		return;
	}

	/* Initialise initrd */
	if ( ( rc = initrd_init() ) != 0 ) {
		/* No way to report failure */
		return;
	}
}

/** Command line and initrd initialisation function */
struct startup_fn runtime_startup_fn __startup_fn ( STARTUP_NORMAL ) = {
	.name = "runtime",
	.startup = runtime_init,
};
