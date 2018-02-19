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
 * Hyper Text Transfer Protocol (HTTP) NTLM authentication
 *
 */

#include <string.h>
#include <errno.h>
#include <ipxe/uri.h>
#include <ipxe/base64.h>
#include <ipxe/ntlm.h>
#include <ipxe/netbios.h>
#include <ipxe/http.h>

struct http_authentication http_ntlm_auth __http_authentication;

/** Workstation name used for NTLM authentication */
static const char http_ntlm_workstation[] = "iPXE";

/**
 * Parse HTTP "WWW-Authenticate" header for NTLM authentication
 *
 * @v http		HTTP transaction
 * @v line		Remaining header line
 * @ret rc		Return status code
 */
static int http_parse_ntlm_auth ( struct http_transaction *http, char *line ) {
	struct http_response_auth_ntlm *rsp = &http->response.auth.ntlm;
	char *copy;
	int len;
	int rc;

	/* Create temporary copy of Base64-encoded challenge message */
	copy = strdup ( line );
	if ( ! copy ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Decode challenge message, overwriting the original */
	len = base64_decode ( copy, line, strlen ( line ) );
	if ( len < 0 ) {
		rc = len;
		DBGC ( http, "HTTP %p could not decode NTLM challenge "
		       "\"%s\": %s\n", http, copy, strerror ( rc ) );
		goto err_decode;
	}

	/* Parse challenge, if present */
	if ( len ) {
		rsp->challenge = ( ( void * ) line );
		if ( ( rc = ntlm_challenge ( rsp->challenge, len,
					     &rsp->info ) ) != 0 ) {
			DBGC ( http, "HTTP %p could not parse NTLM challenge: "
			       "%s\n", http, strerror ( rc ) );
			goto err_challenge;
		}
	}

	/* Allow HTTP request to be retried if the request had not
	 * already tried authentication.  Note that NTLM requires an
	 * additional round trip to obtain the challenge message,
	 * which is not present in the initial WWW-Authenticate.
	 */
	if ( ( http->request.auth.auth == NULL ) ||
	     ( ( http->request.auth.auth == &http_ntlm_auth ) &&
	       ( http->request.auth.ntlm.len == 0 ) && len ) ) {
		http->response.flags |= HTTP_RESPONSE_RETRY;
	}

	/* Success */
	rc = 0;

 err_challenge:
 err_decode:
	free ( copy );
 err_alloc:
	return rc;
}

/**
 * Perform HTTP NTLM authentication
 *
 * @v http		HTTP transaction
 * @ret rc		Return status code
 */
static int http_ntlm_authenticate ( struct http_transaction *http ) {
	struct http_request_auth_ntlm *req = &http->request.auth.ntlm;
	struct http_response_auth_ntlm *rsp = &http->response.auth.ntlm;
	struct ntlm_key key;
	const char *domain;
	char *username;
	const char *password;

	/* If we have no challenge yet, then just send a Negotiate message */
	if ( ! rsp->challenge ) {
		DBGC ( http, "HTTP %p sending NTLM Negotiate\n", http );
		return 0;
	}

	/* Record username */
	if ( ! http->uri->user ) {
		DBGC ( http, "HTTP %p has no username for NTLM "
		       "authentication\n", http );
		return -EACCES;
	}
	req->username = http->uri->user;
	password = ( http->uri->password ? http->uri->password : "" );

	/* Split NetBIOS [domain\]username */
	username = ( ( char * ) req->username );
	domain = netbios_domain ( &username );

	/* Generate key */
	ntlm_key ( domain, username, password, &key );

	/* Generate responses */
	ntlm_response ( &rsp->info, &key, NULL, &req->lm, &req->nt );

	/* Calculate Authenticate message length */
	req->len = ntlm_authenticate_len ( &rsp->info, domain, username,
					   http_ntlm_workstation );

	/* Restore NetBIOS [domain\]username */
	netbios_domain_undo ( domain, username );

	return 0;
}

/**
 * Construct HTTP "Authorization" header for NTLM authentication
 *
 * @v http		HTTP transaction
 * @v buf		Buffer
 * @v len		Length of buffer
 * @ret len		Length of header value, or negative error
 */
static int http_format_ntlm_auth ( struct http_transaction *http,
				   char *buf, size_t len ) {
	struct http_request_auth_ntlm *req = &http->request.auth.ntlm;
	struct http_response_auth_ntlm *rsp = &http->response.auth.ntlm;
	struct ntlm_authenticate *auth;
	const char *domain;
	char *username;
	size_t check;

	/* If we have no challenge yet, then just send a Negotiate message */
	if ( ! rsp->challenge ) {
		return base64_encode ( &ntlm_negotiate,
				       sizeof ( ntlm_negotiate ), buf, len );
	}

	/* Skip allocation if just calculating length */
	if ( ! len )
		return base64_encoded_len ( req->len );

	/* Allocate temporary buffer for Authenticate message */
	auth = malloc ( req->len );
	if ( ! auth )
		return -ENOMEM;

	/* Split NetBIOS [domain\]username */
	username = ( ( char * ) req->username );
	domain = netbios_domain ( &username );

	/* Construct raw Authenticate message */
	check = ntlm_authenticate ( &rsp->info, domain, username,
				    http_ntlm_workstation, &req->lm,
				    &req->nt, auth );
	assert ( check == req->len );

	/* Restore NetBIOS [domain\]username */
	netbios_domain_undo ( domain, username );

	/* Base64-encode Authenticate message */
	len = base64_encode ( auth, req->len, buf, len );

	/* Free raw Authenticate message */
	free ( auth );

	return len;
}

/** HTTP NTLM authentication scheme */
struct http_authentication http_ntlm_auth __http_authentication = {
	.name = "NTLM",
	.parse = http_parse_ntlm_auth,
	.authenticate = http_ntlm_authenticate,
	.format = http_format_ntlm_auth,
};

/* Drag in HTTP authentication support */
REQUIRING_SYMBOL ( http_ntlm_auth );
REQUIRE_OBJECT ( httpauth );
