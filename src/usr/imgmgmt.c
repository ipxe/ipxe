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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ipxe/image.h>
#include <ipxe/downloader.h>
#include <ipxe/monojob.h>
#include <ipxe/open.h>
#include <ipxe/uri.h>
#include <usr/imgmgmt.h>

/** @file
 *
 * Image management
 *
 */

/**
 * Download an image
 *
 * @v image		Image
 * @v uri		URI
 * @v image_register	Action to take upon a successful download
 * @ret rc		Return status code
 */
int imgdownload ( struct image *image, struct uri *uri,
		  int ( * image_register ) ( struct image *image ) ) {
	size_t len = ( unparse_uri ( NULL, 0, uri, URI_ALL ) + 1 );
	char uri_string_redacted[len];
	const char *password;
	int rc;

	/* Set image URI */
	image_set_uri ( image, uri );

	/* Redact password portion of URI, if necessary */
	password = uri->password;
	if ( password )
		uri->password = "***";
	unparse_uri ( uri_string_redacted, sizeof ( uri_string_redacted ),
		      uri, URI_ALL );
	uri->password = password;

	/* Create downloader */
	if ( ( rc = create_downloader ( &monojob, image, image_register,
					LOCATION_URI, uri ) ) != 0 )
		return rc;

	/* Wait for download to complete */
	if ( ( rc = monojob_wait ( uri_string_redacted ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Fetch an image
 *
 * @v image		Image
 * @v uri_string	URI as a string (e.g. "http://www.nowhere.com/vmlinuz")
 * @v image_register	Action to take upon a successful fetch
 * @ret rc		Return status code
 */
int imgfetch ( struct image *image, const char *uri_string,
	       int ( * image_register ) ( struct image *image ) ) {
	struct uri *uri;
	int rc;

	if ( ! ( uri = parse_uri ( uri_string ) ) )
		return -ENOMEM;

	rc = imgdownload ( image, uri, image_register );

	uri_put ( uri );
	return rc;
}

/**
 * Load an image
 *
 * @v image		Image
 * @ret rc		Return status code
 */
int imgload ( struct image *image ) {
	int rc;

	/* Try to load image */
	if ( ( rc = image_autoload ( image ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Execute an image
 *
 * @v image		Image
 * @ret rc		Return status code
 */
int imgexec ( struct image *image ) {
	return image_exec ( image );
}

/**
 * Identify the only loaded image
 *
 * @ret image		Image, or NULL if 0 or >1 images are loaded
 */
struct image * imgautoselect ( void ) {
	struct image *image;
	struct image *selected_image = NULL;
	int flagged_images = 0;

	for_each_image ( image ) {
		if ( image->flags & IMAGE_LOADED ) {
			selected_image = image;
			flagged_images++;
		}
	}

	return ( ( flagged_images == 1 ) ? selected_image : NULL );
}

/**
 * Display status of an image
 *
 * @v image		Executable/loadable image
 */
void imgstat ( struct image *image ) {
	printf ( "%s: %zd bytes", image->name, image->len );
	if ( image->type )
		printf ( " [%s]", image->type->name );
	if ( image->flags & IMAGE_LOADED )
		printf ( " [LOADED]" );
	if ( image->cmdline )
		printf ( " \"%s\"", image->cmdline );
	printf ( "\n" );
}

/**
 * Free an image
 *
 * @v image		Executable/loadable image
 */
void imgfree ( struct image *image ) {
	unregister_image ( image );
}
