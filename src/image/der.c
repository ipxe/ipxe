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
#include <ipxe/der.h>
#include <ipxe/uaccess.h>
#include <ipxe/image.h>

/** @file
 *
 * DER-encoded ASN.1 data
 *
 */

/**
 * Extract ASN.1 object from image
 *
 * @v image		DER image
 * @v offset		Offset within image
 * @v cursor		ASN.1 cursor to fill in
 * @ret next		Offset to next image, or negative error
 *
 * The caller is responsible for eventually calling free() on the
 * allocated ASN.1 cursor.
 */
static int der_asn1 ( struct image *image, size_t offset __unused,
		      struct asn1_cursor **cursor ) {
	void *data;

	/* Allocate cursor and data buffer */
	*cursor = malloc ( sizeof ( **cursor ) + image->len );
	if ( ! *cursor )
		return -ENOMEM;
	data = ( ( ( void * ) *cursor ) + sizeof ( **cursor ) );

	/* Populate cursor and data buffer */
	(*cursor)->data = data;
	(*cursor)->len = image->len;
	copy_from_user ( data, image->data, 0, image->len );

	return image->len;
}

/**
 * Probe DER image
 *
 * @v image		DER image
 * @ret rc		Return status code
 */
static int der_probe ( struct image *image ) {
	struct asn1_cursor cursor;
	uint8_t buf[8];
	size_t extra;
	size_t total;
	int len;
	int rc;

	/* Sanity check: no realistic DER image can be smaller than this */
	if ( image->len < sizeof ( buf ) )
		return -ENOEXEC;

	/* Prepare partial cursor */
	cursor.data = buf;
	cursor.len = sizeof ( buf );
	copy_from_user ( buf, image->data, 0, sizeof ( buf ) );
	extra = ( image->len - sizeof ( buf ) );

	/* Get length of ASN.1 sequence */
	len = asn1_start ( &cursor, ASN1_SEQUENCE, extra );
	if ( len < 0 ) {
		rc = len;
		DBGC ( image, "DER %s is not valid ASN.1: %s\n",
		       image->name, strerror ( rc ) );
		return rc;
	}

	/* Add length of tag and length bytes consumed by asn1_start() */
	total = ( len + ( cursor.data - ( ( void * ) buf ) ) );
	assert ( total <= image->len );

	/* Check that image comprises a single well-formed ASN.1 object */
	if ( total != image->len ) {
		DBGC ( image, "DER %s is not single ASN.1\n", image->name );
		return -ENOEXEC;
	}

	return 0;
}

/** DER image type */
struct image_type der_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "DER",
	.probe = der_probe,
	.asn1 = der_asn1,
};
