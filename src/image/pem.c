/*
 * Copyright (C) 2016 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <ipxe/asn1.h>
#include <ipxe/base64.h>
#include <ipxe/uaccess.h>
#include <ipxe/image.h>
#include <ipxe/pem.h>

/** @file
 *
 * PEM-encoded ASN.1 data
 *
 */

/**
 * Locate next line
 *
 * @v data		PEM data
 * @v len		Length of PEM data
 * @v offset		Starting offset
 * @ret next		Offset to next line
 */
static size_t pem_next ( userptr_t data, size_t len, size_t offset ) {
	off_t eol;

	/* Find and skip next newline character, if any */
	eol = memchr_user ( data, offset, '\n', ( len - offset ) );
	if ( eol < 0 )
		return len;
	return ( eol + 1 );
}

/**
 * Locate boundary marker line
 *
 * @v data		PEM data
 * @v len		Length of PEM data
 * @v offset		Starting offset
 * @v marker		Boundary marker
 * @ret offset		Offset to boundary marker line, or negative error
 */
static int pem_marker ( userptr_t data, size_t len, size_t offset,
			const char *marker ) {
	char buf[ strlen ( marker ) ];

	/* Sanity check */
	assert ( offset <= len );

	/* Scan for marker at start of line */
	while ( offset < len ) {

		/* Check for marker */
		if ( ( len - offset ) < sizeof ( buf ) )
			break;
		copy_from_user ( buf, data, offset, sizeof ( buf ) );
		if ( memcmp ( buf, marker, sizeof ( buf ) ) == 0 )
			return offset;

		/* Move to next line */
		offset = pem_next ( data, len, offset );
		assert ( offset <= len );
	}

	return -ENOENT;
}

/**
 * Extract ASN.1 object from PEM data
 *
 * @v data		PEM data
 * @v len		Length of PEM data
 * @v offset		Offset within data
 * @v cursor		ASN.1 cursor to fill in
 * @ret next		Offset to next object, or negative error
 *
 * The caller is responsible for eventually calling free() on the
 * allocated ASN.1 cursor.
 */
int pem_asn1 ( userptr_t data, size_t len, size_t offset,
	       struct asn1_cursor **cursor ) {
	size_t encoded_len;
	size_t decoded_max_len;
	char *encoded;
	void *decoded;
	int decoded_len;
	int begin;
	int end;
	int rc;

	/* Locate and skip BEGIN marker */
	begin = pem_marker ( data, len, offset, PEM_BEGIN );
	if ( begin < 0 ) {
		rc = begin;
		DBGC ( data, "PEM [%#zx,%#zx) missing BEGIN marker: %s\n",
		       offset, len, strerror ( rc ) );
		goto err_begin;
	}
	begin = pem_next ( data, len, begin );

	/* Locate and skip END marker */
	end = pem_marker ( data, len, begin, PEM_END );
	if ( end < 0 ) {
		rc = end;
		DBGC ( data, "PEM [%#zx,%#zx) missing END marker: %s\n",
		       offset, len, strerror ( rc ) );
		goto err_end;
	}
	encoded_len = ( end - begin );
	end = pem_next ( data, len, end );

	/* Extract Base64-encoded data */
	encoded = malloc ( encoded_len + 1 /* NUL */ );
	if ( ! encoded ) {
		rc = -ENOMEM;
		goto err_alloc_encoded;
	}
	copy_from_user ( encoded, data, begin, encoded_len );
	encoded[encoded_len] = '\0';

	/* Allocate cursor and data buffer */
	decoded_max_len = base64_decoded_max_len ( encoded );
	*cursor = malloc ( sizeof ( **cursor ) + decoded_max_len );
	if ( ! *cursor ) {
		rc = -ENOMEM;
		goto err_alloc_cursor;
	}
	decoded = ( ( ( void * ) *cursor ) + sizeof ( **cursor ) );

	/* Decode Base64-encoded data */
	decoded_len = base64_decode ( encoded, decoded, decoded_max_len );
	if ( decoded_len < 0 ) {
		rc = decoded_len;
		DBGC ( data, "PEM could not decode: %s\n", strerror ( rc ) );
		goto err_decode;
	}
	(*cursor)->data = decoded;
	(*cursor)->len = decoded_len;
	assert ( (*cursor)->len <= decoded_max_len );

	/* Free Base64-encoded data */
	free ( encoded );

	/* Update offset and skip any unencapsulated trailer */
	offset = end;
	if ( pem_marker ( data, len, offset, PEM_BEGIN ) < 0 )
		offset = len;

	return offset;

 err_decode:
	free ( *cursor );
	*cursor = NULL;
 err_alloc_cursor:
	free ( encoded );
 err_alloc_encoded:
 err_end:
 err_begin:
	return rc;
}

/**
 * Probe PEM image
 *
 * @v image		PEM image
 * @ret rc		Return status code
 */
static int pem_image_probe ( struct image *image ) {
	int offset;
	int rc;

	/* Check that image contains a BEGIN marker */
	if ( ( offset = pem_marker ( image->data, image->len, 0,
				     PEM_BEGIN ) ) < 0 ) {
		rc = offset;
		DBGC ( image, "PEM %s has no BEGIN marker: %s\n",
		       image->name, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Extract ASN.1 object from image
 *
 * @v image		PEM image
 * @v offset		Offset within image
 * @v cursor		ASN.1 cursor to fill in
 * @ret next		Offset to next image, or negative error
 *
 * The caller is responsible for eventually calling free() on the
 * allocated ASN.1 cursor.
 */
static int pem_image_asn1 ( struct image *image, size_t offset,
			    struct asn1_cursor **cursor ) {
	int next;
	int rc;

	/* Extract ASN.1 object */
	if ( ( next = pem_asn1 ( image->data, image->len, offset,
				 cursor ) ) < 0 ) {
		rc = next;
		DBGC ( image, "PEM %s could not extract ASN.1: %s\n",
		       image->name, strerror ( rc ) );
		return rc;
	}

	return next;
}

/** PEM image type */
struct image_type pem_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "PEM",
	.probe = pem_image_probe,
	.asn1 = pem_image_asn1,
};
