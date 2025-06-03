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
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <ipxe/deflate.h>
#include <ipxe/image.h>
#include <ipxe/zlib.h>
#include <ipxe/gzip.h>

/** @file
 *
 * gzip compressed images
 *
 */

/**
 * Extract gzip image
 *
 * @v image		Image
 * @v extracted		Extracted image
 * @ret rc		Return status code
 */
static int gzip_extract ( struct image *image, struct image *extracted ) {
	const struct gzip_header *header;
	const struct gzip_extra_header *extra;
	const struct gzip_crc_header *crc;
	const struct gzip_footer *footer;
	const void *data;
	size_t extra_len;
	size_t string_len;
	size_t len;
	unsigned int strings;
	int rc;

	/* Sanity check */
	assert ( image->len >= ( sizeof ( *header ) + sizeof ( *footer ) ) );
	data = image->data;
	len = image->len;

	/* Extract footer */
	assert ( len >= sizeof ( *footer ) );
	len -= sizeof ( *footer );
	footer = ( data + len );

	/* Extract fixed header */
	assert ( len >= sizeof ( *header ) );
	header = data;
	data += sizeof ( *header );
	len -= sizeof ( *header );

	/* Skip extra header, if present */
	if ( header->flags & GZIP_FL_EXTRA ) {
		if ( len < sizeof ( *extra ) ) {
			DBGC ( image, "GZIP %s overlength extra header\n",
			       image->name );
			return -EINVAL;
		}
		extra = data;
		data += sizeof ( *extra );
		len -= sizeof ( *extra );
		extra_len = le16_to_cpu ( extra->len );
		if ( len < extra_len ) {
			DBGC ( image, "GZIP %s overlength extra header\n",
			       image->name );
			return -EINVAL;
		}
		data += extra_len;
		len -= extra_len;
	}

	/* Skip name and/or comment, if present */
	strings = 0;
	if ( header->flags & GZIP_FL_NAME )
		strings++;
	if ( header->flags & GZIP_FL_COMMENT )
		strings++;
	while ( strings-- ) {
		string_len = strnlen ( data, len );
		if ( string_len == len ) {
			DBGC ( image, "GZIP %s overlength name/comment\n",
			       image->name );
			return -EINVAL;
		}
		data += ( string_len + 1 /* NUL */ );
		len -= ( string_len + 1 /* NUL */ );
	}

	/* Skip CRC, if present */
	if ( header->flags & GZIP_FL_HCRC ) {
		if ( len < sizeof ( *crc ) ) {
			DBGC ( image, "GZIP %s overlength CRC header\n",
			       image->name );
			return -EINVAL;
		}
		data += sizeof ( *crc );
		len -= sizeof ( *crc );
	}

	/* Presize extracted image */
	if ( ( rc = image_set_len ( extracted,
				    le32_to_cpu ( footer->len ) ) ) != 0 ) {
		DBGC ( image, "GZIP %s could not presize: %s\n",
		       image->name, strerror ( rc ) );
		return rc;
	}

	/* Decompress image (expanding if necessary) */
	if ( ( rc = zlib_deflate ( DEFLATE_RAW, data, len,
				   extracted ) ) != 0 ) {
		DBGC ( image, "GZIP %s could not decompress: %s\n",
		       image->name, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Probe gzip image
 *
 * @v image		gzip image
 * @ret rc		Return status code
 */
static int gzip_probe ( struct image *image ) {
	const struct gzip_header *header;
	const struct gzip_footer *footer;

	/* Sanity check */
	if ( image->len < ( sizeof ( *header ) + sizeof ( *footer ) ) ) {
		DBGC ( image, "GZIP %s image too short\n", image->name );
		return -ENOEXEC;
	}
	header = image->data;

	/* Check magic header */
	if ( header->magic != cpu_to_be16 ( GZIP_MAGIC ) ) {
		DBGC ( image, "GZIP %s invalid magic\n", image->name );
		return -ENOEXEC;
	}

	return 0;
}

/** gzip image type */
struct image_type gzip_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "gzip",
	.probe = gzip_probe,
	.extract = gzip_extract,
	.exec = image_extract_exec,
};
