/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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
 * CPIO archives
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ipxe/cpio.h>

/**
 * Set field within a CPIO header
 *
 * @v field		Field within CPIO header
 * @v value		Value to set
 */
void cpio_set_field ( char *field, unsigned long value ) {
	char buf[9];

	snprintf ( buf, sizeof ( buf ), "%08lx", value );
	memcpy ( field, buf, 8 );
}

/**
 * Get CPIO image filename
 *
 * @v image		Image
 * @ret len		CPIO filename length (0 for no filename)
 */
size_t cpio_name_len ( struct image *image ) {
	const char *name = cpio_name ( image );
	char *sep;
	size_t len;

	/* Check for existence of CPIO filename */
	if ( ! name )
		return 0;

	/* Locate separator (if any) */
	sep = strchr ( name, ' ' );
	len = ( sep ? ( ( size_t ) ( sep - name ) ) : strlen ( name ) );

	return len;
}

/**
 * Parse CPIO image parameters
 *
 * @v image		Image
 * @v cpio		CPIO header to fill in
 */
static void cpio_parse_cmdline ( struct image *image,
				 struct cpio_header *cpio ) {
	const char *arg;
	char *end;
	unsigned int mode;

	/* Look for "mode=" */
	if ( ( arg = image_argument ( image, "mode=" ) ) ) {
		mode = strtoul ( arg, &end, 8 /* Octal for file mode */ );
		if ( *end && ( *end != ' ' ) ) {
			DBGC ( image, "CPIO %p strange \"mode=\" "
			       "terminator '%c'\n", image, *end );
		}
		cpio_set_field ( cpio->c_mode, ( 0100000 | mode ) );
	}
}

/**
 * Construct CPIO header for image, if applicable
 *
 * @v image		Image
 * @v cpio		CPIO header to fill in
 * @ret len		Length of magic CPIO header (including filename)
 */
size_t cpio_header ( struct image *image, struct cpio_header *cpio ) {
	size_t name_len;
	size_t len;

	/* Get filename length */
	name_len = cpio_name_len ( image );

	/* Images with no filename are assumed to already be CPIO archives */
	if ( ! name_len )
		return 0;

	/* Construct CPIO header */
	memset ( cpio, '0', sizeof ( *cpio ) );
	memcpy ( cpio->c_magic, CPIO_MAGIC, sizeof ( cpio->c_magic ) );
	cpio_set_field ( cpio->c_mode, 0100644 );
	cpio_set_field ( cpio->c_nlink, 1 );
	cpio_set_field ( cpio->c_filesize, image->len );
	cpio_set_field ( cpio->c_namesize, ( name_len + 1 /* NUL */ ) );
	cpio_parse_cmdline ( image, cpio );

	/* Calculate total length */
	len = ( ( sizeof ( *cpio ) + name_len + 1 /* NUL */ + CPIO_ALIGN - 1 )
		& ~( CPIO_ALIGN - 1 ) );

	return len;
}
