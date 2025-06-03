/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Secure Hyper Text Transfer Protocol (HTTPS)
 *
 */

#include <ipxe/open.h>
#include <ipxe/uri.h>
#include <ipxe/tls.h>
#include <ipxe/http.h>
#include <ipxe/features.h>

FEATURE ( FEATURE_PROTOCOL, "HTTPS", DHCP_EB_FEATURE_HTTPS, 1 );

/**
 * Add HTTPS filter
 *
 * @v conn		HTTP connection
 * @ret rc		Return status code
 */
static int https_filter ( struct http_connection *conn ) {

	return add_tls ( &conn->socket, conn->uri->host, NULL, NULL );
}

/** HTTPS URI opener */
struct uri_opener https_uri_opener __uri_opener = {
	.scheme	= "https",
	.open	= http_open_uri,
};

/** HTTP URI scheme */
struct http_scheme https_scheme __http_scheme = {
	.name = "https",
	.port = HTTPS_PORT,
	.filter = https_filter,
};
