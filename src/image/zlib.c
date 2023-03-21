/*
 * Copyright (C) 2021 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <ipxe/deflate.h>
#include <ipxe/uaccess.h>
#include <ipxe/image.h>
#include <ipxe/zlib.h>

/** @file
 *
 * zlib compressed images
 *
 */

/**
 * Extract compressed data to image
 *
 * @v format		Compression format code
 * @v in		Compressed input chunk
 * @v extracted		Extracted image
 * @ret rc		Return status code
 */
int zlib_deflate ( enum deflate_format format, struct deflate_chunk *in,
		   struct image *extracted ) {
	struct deflate *deflate;
	struct deflate_chunk out;
	int rc;

	/* Allocate and initialise decompressor */
	deflate = zalloc ( sizeof ( *deflate ) );
	if ( ! deflate ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Decompress data, (re)allocating if necessary */
	while ( 1 ) {

		/* (Re)initialise decompressor */
		deflate_init ( deflate, format );

		/* (Re)initialise input chunk */
		in->offset = 0;

		/* Initialise output chunk */
		deflate_chunk_init ( &out, extracted->data, 0, extracted->len );

		/* Decompress data */
		if ( ( rc = deflate_inflate ( deflate, in, &out ) ) != 0 ) {
			DBGC ( extracted, "ZLIB %p could not decompress: %s\n",
			       extracted, strerror ( rc ) );
			goto err_inflate;
		}

		/* Check that decompression is valid */
		if ( ! deflate_finished ( deflate ) ) {
			DBGC ( extracted, "ZLIB %p decompression incomplete\n",
			       extracted );
			rc = -EINVAL;
			goto err_unfinished;
		}

		/* Finish if output image size was correct */
		if ( out.offset == extracted->len )
			break;

		/* Otherwise, resize output image and retry */
		if ( ( rc = image_set_len ( extracted, out.offset ) ) != 0 ) {
			DBGC ( extracted, "ZLIB %p could not resize: %s\n",
			       extracted, strerror ( rc ) );
			goto err_set_size;
		}
	}

	/* Success */
	rc = 0;

 err_set_size:
 err_unfinished:
 err_inflate:
	free ( deflate );
 err_alloc:
	return rc;
}

/**
 * Extract zlib image
 *
 * @v image		Image
 * @v extracted		Extracted image
 * @ret rc		Return status code
 */
static int zlib_extract ( struct image *image, struct image *extracted ) {
	struct deflate_chunk in;
	int rc;

	/* Initialise input chunk */
	deflate_chunk_init ( &in, image->data, 0, image->len );

	/* Decompress image */
	if ( ( rc = zlib_deflate ( DEFLATE_ZLIB, &in, extracted ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Probe zlib image
 *
 * @v image		zlib image
 * @ret rc		Return status code
 */
static int zlib_probe ( struct image *image ) {
	union zlib_magic magic;

	/* Sanity check */
	if ( image->len < sizeof ( magic ) ) {
		DBGC ( image, "ZLIB %p image too short\n", image );
		return -ENOEXEC;
	}

	/* Check magic header */
	copy_from_user ( &magic, image->data, 0, sizeof ( magic ) );
	if ( ! zlib_magic_is_valid ( &magic ) ) {
		DBGC ( image, "ZLIB %p invalid magic data\n", image );
		return -ENOEXEC;
	}

	return 0;
}

/** zlib image type */
struct image_type zlib_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "zlib",
	.probe = zlib_probe,
	.extract = zlib_extract,
	.exec = image_extract_exec,
};
