/*
 * Copyright (C) 2015 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Hyper Text Transfer Protocol (HTTP) authentication
 *
 */

#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <ipxe/http.h>

/**
 * Identify authentication scheme
 *
 * @v http		HTTP transaction
 * @v name		Scheme name
 * @ret auth		Authentication scheme, or NULL
 */
static struct http_authentication * http_authentication ( const char *name ) {
	struct http_authentication *auth;

	/* Identify authentication scheme */
	for_each_table_entry ( auth, HTTP_AUTHENTICATIONS ) {
		if ( strcasecmp ( name, auth->name ) == 0 )
			return auth;
	}

	return NULL;
}

/**
 * Parse HTTP "WWW-Authenticate" header
 *
 * @v http		HTTP transaction
 * @v line		Remaining header line
 * @ret rc		Return status code
 */
static int http_parse_www_authenticate ( struct http_transaction *http,
					 char *line ) {
	struct http_authentication *auth;
	char *name;
	int rc;

	/* Get scheme name */
	name = http_token ( &line, NULL );
	if ( ! name ) {
		DBGC ( http, "HTTP %p malformed WWW-Authenticate \"%s\"\n",
		       http, line );
		return -EPROTO;
	}

	/* Identify scheme */
	auth = http_authentication ( name );
	if ( ! auth ) {
		DBGC ( http, "HTTP %p unrecognised authentication scheme "
		       "\"%s\"\n", http, name );
		/* Ignore; the server may offer other schemes */
		return 0;
	}

	/* Use first supported scheme */
	if ( http->response.auth.auth )
		return 0;
	http->response.auth.auth = auth;

	/* Parse remaining header line */
	if ( ( rc = auth->parse ( http, line ) ) != 0 ) {
		DBGC ( http, "HTTP %p could not parse %s WWW-Authenticate "
		       "\"%s\": %s\n", http, name, line, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/** HTTP "WWW-Authenticate" header */
struct http_response_header
http_response_www_authenticate __http_response_header = {
	.name = "WWW-Authenticate",
	.parse = http_parse_www_authenticate,
};

/**
 * Construct HTTP "Authorization" header
 *
 * @v http		HTTP transaction
 * @v buf		Buffer
 * @v len		Length of buffer
 * @ret len		Length of header value, or negative error
 */
static int http_format_authorization ( struct http_transaction *http,
				       char *buf, size_t len ) {
	struct http_authentication *auth = http->request.auth.auth;
	size_t used;
	int auth_len;
	int rc;

	/* Do nothing unless we have an authentication scheme */
	if ( ! auth )
		return 0;

	/* Construct header */
	used = snprintf ( buf, len, "%s ", auth->name );
	auth_len = auth->format ( http, ( buf + used ),
				  ( ( used < len ) ? ( len - used ) : 0 ) );
	if ( auth_len < 0 ) {
		rc = auth_len;
		return rc;
	}
	used += auth_len;

	return used;
}

/** HTTP "Authorization" header */
struct http_request_header http_request_authorization __http_request_header = {
	.name = "Authorization",
	.format = http_format_authorization,
};
