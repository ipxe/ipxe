/*
 * Copyright (C) 2017 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Google Compute Engine (GCE) metadata retrieval
 *
 * For some unspecified "security" reason, the Google Compute Engine
 * metadata server will refuse any requests that do not include the
 * non-standard HTTP header "Metadata-Flavor: Google".
 */

#include <strings.h>
#include <stdio.h>
#include <ipxe/http.h>

/** Metadata host name
 *
 * This is used to identify metadata requests, in the absence of any
 * more robust mechanism.
 */
#define GCE_METADATA_HOST_NAME "metadata.google.internal"

/**
 * Construct HTTP "Metadata-Flavor" header
 *
 * @v http		HTTP transaction
 * @v buf		Buffer
 * @v len		Length of buffer
 * @ret len		Length of header value, or negative error
 */
static int http_format_metadata_flavor ( struct http_transaction *http,
					 char *buf, size_t len ) {

	/* Do nothing unless this appears to be a Google Compute
	 * Engine metadata request.
	 */
	if ( strcasecmp ( http->request.host, GCE_METADATA_HOST_NAME ) != 0 )
		return 0;

	/* Construct host URI */
	return snprintf ( buf, len, "Google" );
}

/** HTTP "Metadata-Flavor" header */
struct http_request_header http_request_metadata_flavor __http_request_header ={
	.name = "Metadata-Flavor",
	.format = http_format_metadata_flavor,
};
