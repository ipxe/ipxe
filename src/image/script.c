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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

/**
 * @file
 *
 * gPXE scripts
 *
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <gpxe/image.h>

struct image_type script_image_type __image_type ( PROBE_NORMAL );

/**
 * Execute script
 *
 * @v image		Script
 * @ret rc		Return status code
 */
static int script_exec ( struct image *image ) {
	size_t offset = 0;
	off_t eol;
	size_t len;
	int rc;

	/* Temporarily de-register image, so that a "boot" command
	 * doesn't throw us into an execution loop.
	 */
	unregister_image ( image );

	while ( offset < image->len ) {
	
		/* Find length of next line, excluding any terminating '\n' */
		eol = memchr_user ( image->data, offset, '\n',
				    ( image->len - offset ) );
		if ( eol < 0 )
			eol = image->len;
		len = ( eol - offset );

		/* Copy line, terminate with NUL, and execute command */
		{
			char cmdbuf[ len + 1 ];

			copy_from_user ( cmdbuf, image->data, offset, len );
			cmdbuf[len] = '\0';
			DBG ( "$ %s\n", cmdbuf );
			if ( ( rc = system ( cmdbuf ) ) != 0 ) {
				DBG ( "Command \"%s\" failed: %s\n",
				      cmdbuf, strerror ( rc ) );
				goto done;
			}
		}
		
		/* Move to next line */
		offset += ( len + 1 );
	}

	rc = 0;
 done:
	/* Re-register image and return */
	register_image ( image );
	return rc;
}

/**
 * Load script into memory
 *
 * @v image		Script
 * @ret rc		Return status code
 */
static int script_load ( struct image *image ) {
	static const char magic[] = "#!gpxe";
	char test[ sizeof ( magic ) - 1 /* NUL */ + 1 /* terminating space */];

	/* Sanity check */
	if ( image->len < sizeof ( test ) ) {
		DBG ( "Too short to be a script\n" );
		return -ENOEXEC;
	}

	/* Check for magic signature */
	copy_from_user ( test, image->data, 0, sizeof ( test ) );
	if ( ( memcmp ( test, magic, ( sizeof ( test ) - 1 ) ) != 0 ) ||
	     ! isspace ( test[ sizeof ( test ) - 1 ] ) ) {
		DBG ( "Invalid magic signature\n" );
		return -ENOEXEC;
	}

	/* This is a script */
	image->type = &script_image_type;

	/* We don't actually load it anywhere; we will pick the lines
	 * out of the image as we need them.
	 */

	return 0;
}

/** Script image type */
struct image_type script_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "script",
	.load = script_load,
	.exec = script_exec,
};
