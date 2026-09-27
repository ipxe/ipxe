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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ipxe/uri.h>
#include <ipxe/open.h>
#include <ipxe/http.h>
#include <ipxe/base16.h>
#include <ipxe/pccrc.h>
#include <ipxe/pccrr.h>

/** @file
 *
 * Peer Content Caching and Retrieval: Retrieval Protocol [MS-PCCRR]
 *
 * The MS-PCCRR specification defines an HTTP POST request/response
 * pair for retrieving an encrypted block from a peer (with the
 * decryption keys provided separately via the content information).
 *
 * The HTTP POST request parameters serve only to identify the block:
 * the actual response body is effectively a static encrypted blob.
 *
 * We define an additional (non-standard) retrieval protocol in which
 * the block is identified solely using the request URI, with the
 * response being the same content that would be returned as the HTTP
 * POST response body.  This allows for encrypted blocks to be
 * retrieved from a static source such as AWS S3 or a local FAT
 * filesystem, without requiring a peer that can understand the
 * retrieval protocol HTTP POST request format.
 *
 * We define the static request URI format as:
 *
 *    <base>/xx/xxyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy-b.blk
 *
 * where
 *
 *    - `xxyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy` is the segment ID (HoHoDK)
 *      as a lower-case hexadecimal string
 *
 *    - `xx` is the first two characters (i.e. the first byte) of the
 *      segment ID
 *
 *    - `b` is the block index within the segment (which will always
 *      be zero when using MS-PCCRC version 2 content information), as
 *      an unpadded decimal string
 *
 *    - the extension `.blk` represents a block file
 *
 * This path format is designed to allow for efficient storage in a
 * local FAT filesystem (by limiting both the number of directories
 * and the number of entries within each directory).
 *
 */

/**
 * Open retrieval protocol connection using HTTP POST
 *
 * @v xfer		Data transfer interface
 * @v location		Peer location
 * @v digestsize	Digest size
 * @v id		Segment identifier
 * @v block		Block index
 * @ret rc		Return status code
 */
static int peerdist_open_post ( struct interface *xfer, const char *location,
				size_t digestsize, const uint8_t *id,
				unsigned int block ) {
	char uri_string[ 7 /* "http://" */ + strlen ( location ) +
			 sizeof ( PEERDIST_MAGIC_PATH /* includes NUL */ ) ];
	peerdist_msg_getblks_t ( digestsize, 1, 0 ) req;
	struct http_request_content content;
	struct uri *uri;
	int rc;

	/* Construct block fetch request */
	memset ( &req, 0, sizeof ( req ) );
	req.getblks.hdr.version.raw = htonl ( PEERDIST_MSG_GETBLKS_VERSION );
	req.getblks.hdr.type = htonl ( PEERDIST_MSG_GETBLKS_TYPE );
	req.getblks.hdr.len = htonl ( sizeof ( req ) );
	req.getblks.hdr.algorithm = htonl ( PEERDIST_MSG_AES_128_CBC );
	req.segment.segment.digestsize = htonl ( digestsize );
	memcpy ( req.segment.id, id, digestsize );
	req.ranges.ranges.count = htonl ( 1 );
	req.ranges.range[0].first = htonl ( block );
	req.ranges.range[0].count = htonl ( 1 );

	/* Construct POST request content */
	memset ( &content, 0, sizeof ( content ) );
	content.data = &req;
	content.len = sizeof ( req );

	/* Construct URI string */
	snprintf ( uri_string, sizeof ( uri_string ),
		   ( "http://%s" PEERDIST_MAGIC_PATH ), location );

	/* Parse URI */
	uri = parse_uri ( uri_string );
	if ( ! uri ) {
		rc = -ENOMEM;
		goto err_uri;
	}

	/* Initiate HTTP POST to retrieve block */
	if ( ( rc = http_open ( xfer, &http_post, uri, NULL,
				&content ) ) != 0 ) {
		DBGC ( xfer, "PCCRR %p could not open %s: %s\n",
		       xfer, uri_string, strerror ( rc ) );
		goto err_open;
	}

 err_open:
	uri_put ( uri );
 err_uri:
	return rc;
}

/** PeerDist retrieval protocol using HTTP POST */
struct peerdist_retrieval peerdist_post = {
	.name = "POST",
	.open = peerdist_open_post,
};

/**
 * Open retrieval protocol connection using HTTP GET or local file
 *
 * @v xfer		Data transfer interface
 * @v location		Peer location
 * @v digestsize	Digest size
 * @v id		Segment identifier
 * @v block		Block index
 * @ret rc		Return status code
 */
static int peerdist_open_get ( struct interface *xfer, const char *location,
			       size_t digestsize, const uint8_t *id,
			       unsigned int block ) {
	char uri_string[ strlen ( location ) + 4 /* "/xx/" */ +
			 ( 2 * PEERDIST_DIGEST_MAX_SIZE ) + 1 /* "-" */ +
			 10 /* block number */ + 4 /* ".blk" */ +
			 1 /* NUL */ ];
	size_t len;
	int rc;

	/* Construct URI string */
	assert ( digestsize <= PEERDIST_DIGEST_MAX_SIZE );
	len = snprintf ( uri_string, sizeof ( uri_string ), "%s/%02x/",
			 location, id[0] );
	assert ( len < sizeof ( uri_string ) );
	len += base16_encode ( id, digestsize, ( uri_string + len ),
			       ( sizeof ( uri_string ) - len ) );
	assert ( len < sizeof ( uri_string ) );
	snprintf ( ( uri_string + len ), ( sizeof ( uri_string ) - len ),
		   "-%d.blk", block );

	/* Open URI */
	if ( ( rc = xfer_open_uri_string ( xfer, uri_string ) ) != 0 ) {
		DBGC ( xfer, "PCCRR %p could not open %s: %s\n",
		       xfer, uri_string, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/** PeerDist retrieval protocol using HTTP GET or local file */
struct peerdist_retrieval peerdist_get = {
	.name = "GET",
	.open = peerdist_open_get,
};
