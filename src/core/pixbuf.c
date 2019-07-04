/*
 * Copyright (C) 2013 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Pixel buffer
 *
 */

#include <stdlib.h>
#include <errno.h>
#include <ipxe/umalloc.h>
#include <ipxe/image.h>
#include <ipxe/pixbuf.h>

/**
 * Free pixel buffer
 *
 * @v refcnt		Reference count
 */
static void free_pixbuf ( struct refcnt *refcnt ) {
	struct pixel_buffer *pixbuf =
		container_of ( refcnt, struct pixel_buffer, refcnt );

	ufree ( pixbuf->data );
	free ( pixbuf );
}

/**
 * Allocate pixel buffer
 *
 * @v width		Width
 * @h height		Height
 * @ret pixbuf		Pixel buffer, or NULL on failure
 */
struct pixel_buffer * alloc_pixbuf ( unsigned int width, unsigned int height ) {
	struct pixel_buffer *pixbuf;

	/* Allocate and initialise structure */
	pixbuf = zalloc ( sizeof ( *pixbuf ) );
	if ( ! pixbuf )
		goto err_alloc_pixbuf;
	ref_init ( &pixbuf->refcnt, free_pixbuf );
	pixbuf->width = width;
	pixbuf->height = height;
	pixbuf->len = ( width * height * sizeof ( uint32_t ) );

	/* Check for multiplication overflow */
	if ( ( width != 0 ) &&
	     ( ( pixbuf->len / sizeof ( uint32_t ) ) / width ) != height ) {
		goto err_overflow;
	}

	/* Allocate pixel data buffer */
	pixbuf->data = umalloc ( pixbuf->len );
	if ( ! pixbuf->data )
		goto err_alloc_data;

	return pixbuf;

 err_alloc_data:
 err_overflow:
	pixbuf_put ( pixbuf );
 err_alloc_pixbuf:
	return NULL;
}

/**
 * Create pixel buffer from image
 *
 * @v image		Image
 * @v pixbuf		Pixel buffer to fill in
 * @ret rc		Return status code
 */
int image_pixbuf ( struct image *image, struct pixel_buffer **pixbuf ) {
	int rc;

	/* Check that this image can be used to create a pixel buffer */
	if ( ! ( image->type && image->type->pixbuf ) )
		return -ENOTSUP;

	/* Try creating pixel buffer */
	if ( ( rc = image->type->pixbuf ( image, pixbuf ) ) != 0 ) {
		DBGC ( image, "IMAGE %s could not create pixel buffer: %s\n",
		       image->name, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/* Drag in objects via image_pixbuf() */
REQUIRING_SYMBOL ( image_pixbuf );

/* Drag in pixel buffer image formats */
REQUIRE_OBJECT ( config_pixbuf );
