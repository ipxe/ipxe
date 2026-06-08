/*
 * Copyright (C) 2026 Huzaifa Ali Zar <huzaifazar@gmail.com>.
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
 * Amazon EC2 Instance Metadata Service v2 (IMDSv2) headers
 *
 * EC2's metadata service requires IMDSv2 session tokens.  A PUT
 * request to the token endpoint must include a TTL header, and
 * subsequent requests must include the resulting token value.
 *
 * This mirrors the approach used for Google Compute Engine metadata
 * in httpgce.c.
 */

#include <string.h>
#include <stdio.h>
#include <ipxe/http.h>
#include <ipxe/settings.h>

/** EC2 metadata IPv4 address */
#define EC2_METADATA_HOST_ADDR "169.254.169.254"

/** Token TTL in seconds (6 hours) */
#define EC2_TOKEN_TTL "21600"

/** EC2 IMDSv2 token setting */
static const struct setting ec2token_setting = {
	.name = "ec2token",
	.type = &setting_type_string,
};

/**
 * Construct HTTP "X-aws-ec2-metadata-token-ttl-seconds" header
 *
 * @v http		HTTP transaction
 * @v buf		Buffer
 * @v len		Length of buffer
 * @ret len		Length of header value, or negative error
 */
static int
http_format_ec2_token_ttl ( struct http_transaction *http,
			    char *buf, size_t len ) {

	/* Skip if not an EC2 metadata request */
	if ( strcmp ( http->request.host, EC2_METADATA_HOST_ADDR ) != 0 )
		return 0;

	/* Skip if not a PUT request */
	if ( http->request.method != &http_put )
		return 0;

	/* Construct header value */
	return snprintf ( buf, len, "%s", EC2_TOKEN_TTL );
}

/** HTTP "X-aws-ec2-metadata-token-ttl-seconds" header */
struct http_request_header
http_request_ec2_token_ttl __http_request_header = {
	.name = "X-aws-ec2-metadata-token-ttl-seconds",
	.format = http_format_ec2_token_ttl,
};

/**
 * Construct HTTP "X-aws-ec2-metadata-token" header
 *
 * @v http		HTTP transaction
 * @v buf		Buffer
 * @v len		Length of buffer
 * @ret len		Length of header value, or negative error
 */
static int
http_format_ec2_token ( struct http_transaction *http,
			char *buf, size_t len ) {
	int token_len;

	/* Skip if not an EC2 metadata request */
	if ( strcmp ( http->request.host, EC2_METADATA_HOST_ADDR ) != 0 )
		return 0;

	/* Skip if this is a PUT request (token acquisition) */
	if ( http->request.method == &http_put )
		return 0;

	/* Fetch token from setting */
	token_len = fetch_string_setting ( NULL, &ec2token_setting,
					   buf, len );
	if ( token_len < 1 )
		return 0;

	return token_len;
}

/** HTTP "X-aws-ec2-metadata-token" header */
struct http_request_header
http_request_ec2_token __http_request_header = {
	.name = "X-aws-ec2-metadata-token",
	.format = http_format_ec2_token,
};
