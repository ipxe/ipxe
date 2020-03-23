/*
 * Copyright (C) 2020 Miao Wang <shankerwangmiao@gmail.com>.
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

/**
 * @file
 *
 * gzip files
 *
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <ipxe/image.h>
#include <ipxe/uaccess.h>
#include <ipxe/umalloc.h>
#include <ipxe/deflate.h>

/**
 * Unpack gzip image and execute it
 *
 * @v image		Gzip image
 * @ret rc		Return status code
 */
static int gzip_exec ( struct image *image ) {
	struct image *unzipped;
	struct deflate_chunk in, out;
	struct deflate deflate;
	int rc = 0;
	userptr_t gzip_window;
	
	/* Inflate into an empty chunk to calc the size after inflation */
	deflate_init ( &deflate, DEFLATE_GZIP );
	deflate.window = gzip_window = umalloc ( GZIP_WSIZE );
	if ( ! deflate.window ) {
		DBGC ( image, "GZIP %s could not allocate the buffer for inflation\n",
		       image->name );
		rc = -ENOMEM;
		goto err_wndalloc;
	}
	deflate_chunk_init ( &in, image->data, 0, image->len );
	deflate_chunk_init ( &out, UNULL, 0, 0 );
	printf ( "GZIP: trying to decompress: %s ", image->name );
	if ( ( rc = deflate_inflate ( &deflate, &in, &out ) ) != 0 ) {
		printf ( "[failed]\n" );
		printf ( "GZIP: %s could not decompress: %s\n",
		       image->name, strerror ( rc ) );
		goto err_deflate;
	}
	if ( ! deflate_finished ( &deflate ) ) {
		printf ( "[failed]\n" );
		printf ( "GZIP: %s unexpected EOF\n", image->name );
		rc = -EINVAL;
		goto err_deflate;
	}
	printf ( "%zd bytes\n", out.offset );
	
	/* Allocate the new image */
	unzipped = alloc_image( image->uri );
	if ( ! unzipped ){
		DBGC ( image, "GZIP %s could not allocate the new image\n",
		       image->name );
		rc = -ENOMEM;
		goto err_alloc_img;
	}
	
	/* Construct the new image */
	if ( ( rc = image_set_name ( unzipped, image->name ) ) != 0 ) {
		DBGC ( image, "GZIP %s could not name the new image\n",
		       image->name );
		goto err_copy_prop;
	};
	if ( ( rc = image_set_cmdline ( unzipped, image->cmdline ) ) != 0 ) {
		DBGC ( image, "GZIP %s could not set cmdline for the new image\n",
		       image->name );
		goto err_copy_prop;
	};
	/* Allocate buffer */
	unzipped->data = umalloc ( out.offset );
	if ( ! unzipped->data ) {
		DBGC ( image, "GZIP %s could not allocate data buffer\n",
		       image->name );
		rc = -ENOMEM;
		goto err_alloc_buf;
	}
	unzipped->len = out.offset;
	
	/* Decompress */
	printf ( "GZIP: decompressing: %s ", image->name );
	deflate_init ( &deflate, DEFLATE_GZIP );
	deflate.window = gzip_window;
	deflate_chunk_init ( &in, image->data, 0, image->len);
	deflate_chunk_init ( &out, unzipped->data, 0, unzipped->len);
	/* Since we've already inflated, it cannot fail */
	rc = deflate_inflate ( &deflate, &in, &out );
	assert ( rc == 0 );
	assert ( deflate_finished ( &deflate ) );
	assert ( out.offset == unzipped->len );
	printf ( "[ok]\n" );
	
	if ( ( rc = register_image ( unzipped ) ) != 0 ) {
		DBGC ( image, "GZIP %s could not register the new image\n",
		       image->name );
		goto err_reg;
	}
	
	unregister_image ( image );
	
	if ( ( rc = image_replace ( unzipped ) ) != 0 ) {
		DBGC ( image, "GZIP %s could not replace with the new image\n",
		       image->name );
		goto err_replace;
	}
	
	/* Success */
	rc = 0;
	
	err_replace:
	err_reg:
	err_alloc_buf:
	err_copy_prop:
	image_put ( unzipped );
	err_alloc_img:
	err_deflate:
	ufree ( deflate.window );
	err_wndalloc:
	return rc;
}

/**
 * Probe gzip image
 *
 * @v image		Gzip image
 * @ret rc		Return status code
 */
static int gzip_probe ( struct image *image ) {
	uint8_t gzip_header[GZIP_HEADER_BYTES];
	
	/* Sanity check */
	if ( image->len < sizeof ( gzip_header ) ) {
		DBGC ( image, "GZIP %s is too short\n", image->name );
		return -ENOEXEC;
	}
	
	/* Extract header */
	copy_from_user ( gzip_header, image->data, 0, sizeof ( gzip_header ) );
	
	if ( gzip_header [0] != 0x1f || gzip_header [1] != 0x8b ) {
		DBGC ( image, "GZIP %s invalid GZIP format\n", image->name );
		return -ENOEXEC;
	}
	if ( gzip_header [2] != GZIP_HEADER_CM_DEFLATE ) {
		DBGC ( image, "GZIP %s unsupported GZIP "
					 "compression method %d\n", image->name, gzip_header [2] );
		return -ENOTSUP;
	}
	
	return 0;
}

/** Gzip image type */
struct image_type gzip_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "gzip",
	.probe = gzip_probe,
	.exec = gzip_exec,
};

