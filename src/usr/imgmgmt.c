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
 * @v uri		URI
 * @v name		Image name, or NULL to use default
 * @v cmdline		Command line, or NULL for no command line
 * @v action		Action to take upon a successful download, or NULL
 * @ret rc		Return status code
 */
int imgdownload ( struct uri *uri, const char *name, const char *cmdline,
		  int ( * action ) ( struct image *image ) ) {
	struct image *image;
	size_t len = ( unparse_uri ( NULL, 0, uri, URI_ALL ) + 1 );
	char uri_string_redacted[len];
	const char *password;
	int rc;

	/* Allocate image */
	image = alloc_image();
	if ( ! image ) {
		rc = -ENOMEM;
		goto err_alloc_image;
	}

	/* Set image name */
	if ( name )
		image_set_name ( image, name );

	/* Set image URI */
	image_set_uri ( image, uri );

	/* Set image command line */
	image_set_cmdline ( image, cmdline );

	/* Redact password portion of URI, if necessary */
	password = uri->password;
	if ( password )
		uri->password = "***";
	unparse_uri ( uri_string_redacted, sizeof ( uri_string_redacted ),
		      uri, URI_ALL );
	uri->password = password;

	/* Create downloader */
	if ( ( rc = create_downloader ( &monojob, image, LOCATION_URI,
					uri ) ) != 0 ) {
		goto err_create_downloader;
	}

	/* Wait for download to complete */
	if ( ( rc = monojob_wait ( uri_string_redacted ) ) != 0 )
		goto err_monojob_wait;

	/* Register image */
	if ( ( rc = register_image ( image ) ) != 0 )
		goto err_register_image;

	/* Drop local reference to image.  Image is guaranteed to
	 * remain in scope since it is registered.
	 */
	image_put ( image );

	/* Carry out specified post-download action, if applicable */
	return ( action ? action ( image ) : 0 );

 err_register_image:
 err_monojob_wait:
 err_create_downloader:
	image_put ( image );
 err_alloc_image:
	return rc;
}

/**
 * Download an image
 *
 * @v uri_string	URI as a string (e.g. "http://www.nowhere.com/vmlinuz")
 * @v name		Image name, or NULL to use default
 * @v cmdline		Command line, or NULL for no command line
 * @v action		Action to take upon a successful download
 * @ret rc		Return status code
 */
int imgdownload_string ( const char *uri_string, const char *name,
			 const char *cmdline,
			 int ( * action ) ( struct image *image ) ) {
	struct uri *uri;
	int rc;

	if ( ! ( uri = parse_uri ( uri_string ) ) )
		return -ENOMEM;

	rc = imgdownload ( uri, name, cmdline, action );

	uri_put ( uri );
	return rc;
}

/**
 * Display status of an image
 *
 * @v image		Executable/loadable image
 */
void imgstat ( struct image *image ) {
	printf ( "%s : %zd bytes", image->name, image->len );
	if ( image->type )
		printf ( " [%s]", image->type->name );
	if ( image->flags & IMAGE_SELECTED )
		printf ( " [SELECTED]" );
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
