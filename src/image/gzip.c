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
	struct gzip_header header;
	struct gzip_extra_header extra;
	struct gzip_crc_header crc;
	struct gzip_footer footer;
	struct deflate_chunk in;
	unsigned int strings;
	size_t offset;
	size_t len;
	off_t nul;
	int rc;

	/* Sanity check */
	assert ( image->len >= ( sizeof ( header ) + sizeof ( footer ) ) );

	/* Extract footer */
	len = ( image->len - sizeof ( footer ) );
	copy_from_user ( &footer, image->data, len, sizeof ( footer ) );

	/* Extract fixed header */
	copy_from_user ( &header, image->data, 0, sizeof ( header ) );
	offset = sizeof ( header );
	assert ( offset <= ( image->len - sizeof ( footer ) ) );

	/* Skip extra header, if present */
	if ( header.flags & GZIP_FL_EXTRA ) {
		copy_from_user ( &extra, image->data, offset,
				 sizeof ( extra ) );
		offset += sizeof ( extra );
		offset += le16_to_cpu ( extra.len );
		if ( offset > len ) {
			DBGC ( image, "GZIP %p overlength extra header\n",
			       image );
			return -EINVAL;
		}
	}
	assert ( offset <= ( image->len - sizeof ( footer ) ) );

	/* Skip name and/or comment, if present */
	strings = 0;
	if ( header.flags & GZIP_FL_NAME )
		strings++;
	if ( header.flags & GZIP_FL_COMMENT )
		strings++;
	while ( strings-- ) {
		nul = memchr_user ( image->data, offset, 0, ( len - offset ) );
		if ( nul < 0 ) {
			DBGC ( image, "GZIP %p overlength name/comment\n",
			       image );
			return -EINVAL;
		}
		offset = ( nul + 1 /* NUL */ );
	}
	assert ( offset <= ( image->len - sizeof ( footer ) ) );

	/* Skip CRC, if present */
	if ( header.flags & GZIP_FL_HCRC ) {
		offset += sizeof ( crc );
		if ( offset > len ) {
			DBGC ( image, "GZIP %p overlength CRC header\n",
			       image );
			return -EINVAL;
		}
	}

	/* Initialise input chunk */
	deflate_chunk_init ( &in, userptr_add ( image->data, offset ), 0, len );

	/* Presize extracted image */
	if ( ( rc = image_set_len ( extracted,
				    le32_to_cpu ( footer.len ) ) ) != 0 ) {
		DBGC ( image, "GZIP %p could not presize: %s\n",
		       image, strerror ( rc ) );
		return rc;
	}

	/* Decompress image (expanding if necessary) */
	if ( ( rc = zlib_deflate ( DEFLATE_RAW, &in, extracted ) ) != 0 ) {
		DBGC ( image, "GZIP %p could not decompress: %s\n",
		       image, strerror ( rc ) );
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
	struct gzip_header header;
	struct gzip_footer footer;

	/* Sanity check */
	if ( image->len < ( sizeof ( header ) + sizeof ( footer ) ) ) {
		DBGC ( image, "GZIP %p image too short\n", image );
		return -ENOEXEC;
	}

	/* Check magic header */
	copy_from_user ( &header.magic, image->data, 0,
			 sizeof ( header.magic ) );
	if ( header.magic != cpu_to_be16 ( GZIP_MAGIC ) ) {
		DBGC ( image, "GZIP %p invalid magic\n", image );
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
