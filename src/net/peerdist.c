/*
 * Copyright (C) 2015 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdio.h>
#include <ipxe/http.h>
#include <ipxe/settings.h>
#include <ipxe/peermux.h>

/** @file
 *
 * Peer Content Caching and Retrieval (PeerDist) protocol
 *
 * This is quite possibly the ugliest protocol I have ever had the
 * misfortune to encounter, and I've encountered multicast TFTP.
 */

/** PeerDist is globally enabled */
static long peerdist_enabled = 1;

/**
 * Check whether or not to support PeerDist encoding for this request
 *
 * @v http		HTTP transaction
 * @ret supported	PeerDist encoding is supported for this request
 */
static int http_peerdist_supported ( struct http_transaction *http ) {

	/* Allow PeerDist to be globally enabled/disabled */
	if ( ! peerdist_enabled )
		return 0;

	/* Support PeerDist encoding only if we can directly access an
	 * underlying data transfer buffer.  Direct access is required
	 * in order to support decryption of data received via the
	 * retrieval protocol (which provides the AES initialisation
	 * vector only after all of the encrypted data has been
	 * received).
	 *
	 * This test simultaneously ensures that we do not attempt to
	 * use PeerDist encoding on a request which is itself a
	 * PeerDist individual block download, since the individual
	 * block downloads do not themselves provide direct access to
	 * an underlying data transfer buffer.
	 */
	return ( xfer_buffer ( &http->xfer ) != NULL );
}

/**
 * Format HTTP "X-P2P-PeerDist" header
 *
 * @v http		HTTP transaction
 * @v buf		Buffer
 * @v len		Length of buffer
 * @ret len		Length of header value, or negative error
 */
static int http_format_p2p_peerdist ( struct http_transaction *http,
				      char *buf, size_t len ) {
	int supported = http_peerdist_supported ( http );
	int missing;

	/* PeerDist wants us to inform the server whenever we make a
	 * request for data that was missing from local peers
	 * (presumably for statistical purposes only).  We use the
	 * heuristic of assuming that the combination of "this request
	 * may not itself use PeerDist content encoding" and "this is
	 * a range request" probably indicates that we are making a
	 * PeerDist block raw range request for missing data.
	 */
	missing = ( http->request.range.len && ( ! supported ) );

	/* Omit header if PeerDist encoding is not supported and we
	 * are not reporting a missing data request.
	 */
	if ( ! ( supported || missing ) )
		return 0;

	/* Construct header */
	return snprintf ( buf, len, "Version=1.1%s",
			  ( missing ? ", MissingDataRequest=true" : "" ) );
}

/** HTTP "X-P2P-PeerDist" header */
struct http_request_header http_request_p2p_peerdist __http_request_header = {
	.name = "X-P2P-PeerDist",
	.format = http_format_p2p_peerdist,
};

/**
 * Format HTTP "X-P2P-PeerDistEx" header
 *
 * @v http		HTTP transaction
 * @v buf		Buffer
 * @v len		Length of buffer
 * @ret len		Length of header value, or negative error
 */
static int http_format_p2p_peerdistex ( struct http_transaction *http,
					char *buf, size_t len ) {
	int supported = http_peerdist_supported ( http );

	/* Omit header if PeerDist encoding is not supported */
	if ( ! supported )
		return 0;

	/* Construct header */
	return snprintf ( buf, len, ( "MinContentInformation=1.0, "
				      "MaxContentInformation=2.0" ) );
}

/** HTTP "X-P2P-PeerDist" header */
struct http_request_header http_request_p2p_peerdistex __http_request_header = {
	.name = "X-P2P-PeerDistEx",
	.format = http_format_p2p_peerdistex,
};

/**
 * Initialise PeerDist content encoding
 *
 * @v http		HTTP transaction
 * @ret rc		Return status code
 */
static int http_peerdist_init ( struct http_transaction *http ) {

	return peermux_filter ( &http->content, &http->transfer, http->uri );
}

/** PeerDist HTTP content encoding */
struct http_content_encoding peerdist_encoding __http_content_encoding = {
	.name = "peerdist",
	.supported = http_peerdist_supported,
	.init = http_peerdist_init,
};

/** PeerDist enabled setting */
const struct setting peerdist_setting __setting ( SETTING_MISC, peerdist ) = {
	.name = "peerdist",
	.description = "PeerDist enabled",
	.type = &setting_type_int8,
};

/**
 * Apply PeerDist settings
 *
 * @ret rc		Return status code
 */
static int apply_peerdist_settings ( void ) {

	/* Fetch global PeerDist enabled setting */
	if ( fetch_int_setting ( NULL, &peerdist_setting,
				 &peerdist_enabled ) < 0 ) {
		peerdist_enabled = 1;
	}
	DBGC ( &peerdist_enabled, "PEERDIST is %s\n",
	       ( peerdist_enabled ? "enabled" : "disabled" ) );

	return 0;
}

/** PeerDist settings applicator */
struct settings_applicator peerdist_applicator __settings_applicator = {
	.apply = apply_peerdist_settings,
};
