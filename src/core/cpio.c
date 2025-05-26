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

/** CPIO default file mode */
#define CPIO_DEFAULT_MODE 0644

/** CPIO directory mode */
#define CPIO_DEFAULT_DIR_MODE 0755

/**
 * Set field within a CPIO header
 *
 * @v field		Field within CPIO header
 * @v value		Value to set
 */
static void cpio_set_field ( char *field, unsigned long value ) {
	char buf[9];

	snprintf ( buf, sizeof ( buf ), "%08lx", value );
	memcpy ( field, buf, 8 );
}

/**
 * Get maximum number of CPIO headers (i.e. number of path components)
 *
 * @v image		Image
 * @ret max		Maximum number of CPIO headers
 */
static unsigned int cpio_max ( struct image *image ) {
	const char *name = cpio_name ( image );
	unsigned int max = 0;
	char c;
	char p;

	/* Check for existence of CPIO filename */
	if ( ! name )
		return 0;

	/* Count number of path components */
	for ( p = '/' ; ( ( ( c = *(name++) ) ) && ( c != ' ' ) ) ; p = c ) {
		if ( ( p == '/' ) && ( c != '/' ) )
			max++;
	}

	return max;
}

/**
 * Get CPIO image filename
 *
 * @v image		Image
 * @v depth		Path depth
 * @ret len		Filename length
 */
static size_t cpio_name_len ( struct image *image, unsigned int depth ) {
	const char *name = cpio_name ( image );
	size_t len;
	char c;
	char p;

	/* Sanity checks */
	assert ( name != NULL );
	assert ( depth > 0 );

	/* Calculate length up to specified path depth */
	for ( len = 0, p = '/' ; ( ( ( c = name[len] ) ) && ( c != ' ' ) ) ;
	      len++, p = c ) {
		if ( ( c == '/' ) && ( p != '/' ) && ( --depth == 0 ) )
			break;
	}

	return len;
}

/**
 * Parse CPIO image parameters
 *
 * @v image		Image
 * @v mode		Mode to fill in
 * @v count		Number of CPIO headers to fill in
 */
static void cpio_parse_cmdline ( struct image *image, unsigned int *mode,
				 unsigned int *count ) {
	const char *arg;
	char *end;

	/* Set default values */
	*mode = CPIO_DEFAULT_MODE;
	*count = 1;

	/* Parse "mode=...", if present */
	if ( ( arg = image_argument ( image, "mode=" ) ) ) {
		*mode = strtoul ( arg, &end, 8 /* Octal for file mode */ );
		if ( *end && ( *end != ' ' ) ) {
			DBGC ( image, "CPIO %s strange \"mode=\" "
			       "terminator '%c'\n", image->name, *end );
		}
	}

	/* Parse "mkdir=...", if present */
	if ( ( arg = image_argument ( image, "mkdir=" ) ) ) {
		*count += strtoul ( arg, &end, 10 );
		if ( *end && ( *end != ' ' ) ) {
			DBGC ( image, "CPIO %s strange \"mkdir=\" "
			       "terminator '%c'\n", image->name, *end );
		}
	}

	/* Allow "mkdir=-1" to request creation of full directory tree */
	if ( ! *count )
		*count = -1U;
}

/**
 * Construct CPIO header for image, if applicable
 *
 * @v image		Image
 * @v index		CPIO header index
 * @v cpio		CPIO header to fill in
 * @ret len		Length of CPIO header (including name, excluding NUL)
 */
size_t cpio_header ( struct image *image, unsigned int index,
		     struct cpio_header *cpio ) {
	const char *name = cpio_name ( image );
	unsigned int mode;
	unsigned int count;
	unsigned int max;
	unsigned int depth;
	unsigned int i;
	size_t name_len;
	size_t len;

	/* Parse command line arguments */
	cpio_parse_cmdline ( image, &mode, &count );

	/* Determine number of CPIO headers to be constructed */
	max = cpio_max ( image );
	if ( count > max )
		count = max;

	/* Determine path depth of this CPIO header */
	if ( index >= count )
		return 0;
	depth = ( max - count + index + 1 );

	/* Get filename length */
	name_len = cpio_name_len ( image, depth );

	/* Set directory mode or file mode as appropriate */
	if ( name[name_len] == '/' ) {
		mode = ( CPIO_MODE_DIR | CPIO_DEFAULT_DIR_MODE );
	} else {
		mode |= CPIO_MODE_FILE;
	}

	/* Set length on final header */
	len = ( ( depth < max ) ? 0 : image->len );

	/* Construct CPIO header */
	memset ( cpio, '0', sizeof ( *cpio ) );
	memcpy ( cpio->c_magic, CPIO_MAGIC, sizeof ( cpio->c_magic ) );
	cpio_set_field ( cpio->c_mode, mode );
	cpio_set_field ( cpio->c_nlink, 1 );
	cpio_set_field ( cpio->c_filesize, len );
	cpio_set_field ( cpio->c_namesize, ( name_len + 1 /* NUL */ ) );
	DBGC ( image, "CPIO %s %d/%d \"", image->name, depth, max );
	for ( i = 0 ; i < name_len ; i++ )
		DBGC ( image, "%c", name[i] );
	DBGC ( image, "\"\n" );
	DBGC2_HDA ( image, 0, cpio, sizeof ( *cpio ) );

	/* Calculate total length */
	return ( sizeof ( *cpio ) + name_len );
}
