/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
FILE_SECBOOT ( PERMITTED );

#include <string.h>
#include <strings.h>
#include <errno.h>
#include <ipxe/base64.h>
#include <ipxe/blob.h>
#include <ipxe/open.h>
#include <ipxe/datauri.h>

/** @file
 *
 * Data URIs
 *
 * The "data" URI scheme is defined in RFC 2397.  In the interest of
 * reducing code size, we support only a practical subset of the
 * specification.  Media types will be ignored (and must not contain a
 * literal comma character).  Base64 encoding is supported.
 *
 * URI encoding will already have been stripped by the URI parser.
 * Special characters may therefore be used in the URI string
 * (e.g. "hello%20world").  However, URI-encoded NUL bytes (%00) will
 * not work as expected, since they will be interpreted as terminating
 * the URI string.  If NUL bytes are required, then Base64 encoding
 * must be used instead.
 */

/**
 * Parse data URI
 *
 * @v uri		Data URI
 * @v buf		Buffer to fill in
 * @ret len		Length of data, or negative error
 *
 * The buffer length must be at least the size reported by
 * datauri_max_len().
 */
int datauri_parse ( struct uri *uri, void *buf ) {
	static const char b64marker[7] = ";base64";
	const char *comma;
	const char *encoded;
	size_t prefix_len;
	int len;
	int rc;

	/* Sanity check */
	if ( ! uri->opaque ) {
		DBGC ( uri, "DATA %p has no opaque part\n", uri );
		return -EINVAL;
	}

	/* Locate encoded string */
	comma = strchr ( uri->opaque, ',' );
	if ( ! comma ) {
		DBGC ( uri, "DATA %p has no comma separator\n", uri );
		return -EINVAL;
	}
	prefix_len = ( comma - uri->opaque );
	encoded = ( comma + 1 );
	len = strlen ( encoded );

	/* Decode string */
	if ( ( prefix_len >= sizeof ( b64marker ) ) &&
	     ( strncasecmp ( ( comma - sizeof ( b64marker ) ), b64marker,
			     sizeof ( b64marker ) ) == 0 ) ) {

		/* Decode Base64 string */
		len = base64_decode ( encoded, buf, len );
		if ( len < 0 ) {
			rc = len;
			DBGC ( uri, "DATA %p could not decode Base64: %s\n",
			       uri, strerror ( rc ) );
			return rc;
		}

	} else {

		/* Copy raw string */
		strcpy ( buf, encoded );
	}

	DBGC ( uri, "DATA %p decoded \"%s\":\n", uri, uri->opaque );
	DBGC_HDA ( uri, 0, buf, len );
	return len;
}

/**
 * Open data URI
 *
 * @v xfer		Data transfer interface
 * @v uri		URI
 * @ret rc		Return status code
 */
static int datauri_open ( struct interface *xfer, struct uri *uri ) {
	void *data;
	int len;
	int rc;

	/* Allocate space for parsed data */
	data = malloc ( datauri_max_len ( uri ) );
	if ( ! data ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Parse data */
	len = datauri_parse ( uri, data );
	if ( len < 0 )  {
		rc = len;
		goto err_parse;
	}

	/* Open downloadable blob */
	if ( ( rc = blob_open ( xfer, data, len ) ) != 0 )
		goto err_open;

 err_open:
 err_parse:
	free ( data );
 err_alloc:
	return rc;
}

/** Data URI opener */
struct uri_opener data_uri_opener __uri_opener = {
	.scheme = "data",
	.open = datauri_open,
};
