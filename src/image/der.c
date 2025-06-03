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
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <ipxe/asn1.h>
#include <ipxe/der.h>
#include <ipxe/image.h>

/** @file
 *
 * DER-encoded ASN.1 data
 *
 */

/**
 * Extract ASN.1 object from DER data
 *
 * @v data		DER data
 * @v len		Length of DER data
 * @v offset		Offset within data
 * @v cursor		ASN.1 cursor to fill in
 * @ret next		Offset to next object, or negative error
 *
 * The caller is responsible for eventually calling free() on the
 * allocated ASN.1 cursor.
 */
int der_asn1 ( const void *data, size_t len, size_t offset,
	       struct asn1_cursor **cursor ) {
	size_t remaining;
	void *raw;

	/* Sanity check */
	assert ( offset <= len );
	remaining = ( len - offset );

	/* Allocate cursor and data buffer */
	*cursor = malloc ( sizeof ( **cursor ) + remaining );
	if ( ! *cursor )
		return -ENOMEM;
	raw = ( ( ( void * ) *cursor ) + sizeof ( **cursor ) );

	/* Populate cursor and data buffer */
	(*cursor)->data = raw;
	(*cursor)->len = remaining;
	memcpy ( raw, ( data + offset ), remaining );

	/* Shrink cursor */
	asn1_shrink_any ( *cursor );

	return ( offset + (*cursor)->len );
}

/**
 * Probe DER image
 *
 * @v image		DER image
 * @ret rc		Return status code
 */
static int der_image_probe ( struct image *image ) {
	struct asn1_cursor cursor;
	int rc;

	/* Prepare cursor */
	cursor.data = image->data;
	cursor.len = image->len;

	/* Check that image begins with an ASN.1 sequence object */
	if ( ( rc = asn1_skip ( &cursor, ASN1_SEQUENCE ) ) != 0 ) {
		DBGC ( image, "DER %s is not valid ASN.1: %s\n",
		       image->name, strerror ( rc ) );
		return rc;
	}

	/* Check that image comprises a single well-formed ASN.1 object */
	if ( cursor.len ) {
		DBGC ( image, "DER %s is not single ASN.1\n", image->name );
		return -ENOEXEC;
	}

	return 0;
}

/**
 * Extract ASN.1 object from DER image
 *
 * @v image		DER image
 * @v offset		Offset within image
 * @v cursor		ASN.1 cursor to fill in
 * @ret next		Offset to next image, or negative error
 *
 * The caller is responsible for eventually calling free() on the
 * allocated ASN.1 cursor.
 */
static int der_image_asn1 ( struct image *image, size_t offset,
			    struct asn1_cursor **cursor ) {
	int next;
	int rc;

	/* Extract ASN.1 object */
	if ( ( next = der_asn1 ( image->data, image->len, offset,
				 cursor ) ) < 0 ) {
		rc = next;
		DBGC ( image, "DER %s could not extract ASN.1: %s\n",
		       image->name, strerror ( rc ) );
		return rc;
	}

	return next;
}

/** DER image type */
struct image_type der_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "DER",
	.probe = der_image_probe,
	.asn1 = der_image_asn1,
};
