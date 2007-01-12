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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**
 * @file
 *
 * Fetch file as executable/loadable image
 *
 */

#include <vsprintf.h>
#include <gpxe/emalloc.h>
#include <gpxe/ebuffer.h>
#include <gpxe/image.h>
#include <usr/fetch.h>

#include <byteswap.h>
#include <gpxe/dhcp.h>
#include <gpxe/tftp.h>
#include <gpxe/http.h>

/**
 * Fetch file
 *
 * @v filename		Filename to fetch
 * @ret data		Loaded file
 * @ret len		Length of loaded file
 * @ret rc		Return status code
 *
 * Fetch file to an external buffer allocated with emalloc().  The
 * caller is responsible for eventually freeing the buffer with
 * efree().
 */
int fetch ( const char *filename, userptr_t *data, size_t *len ) {
	struct buffer buffer;
	int rc;

	/* Allocate an expandable buffer to hold the file */
	if ( ( rc = ebuffer_alloc ( &buffer, 0 ) ) != 0 )
		return rc;

#warning "Temporary pseudo-URL parsing code"

	/* Retrieve the file */
	union {
		struct sockaddr_tcpip st;
		struct sockaddr_in sin;
	} server;
	struct tftp_session tftp;
	struct http_request http;
	struct async_operation *aop;

	memset ( &tftp, 0, sizeof ( tftp ) );
	memset ( &http, 0, sizeof ( http ) );
	memset ( &server, 0, sizeof ( server ) );
	server.sin.sin_family = AF_INET;
	find_global_dhcp_ipv4_option ( DHCP_EB_SIADDR,
				       &server.sin.sin_addr );


#if 0
	server.sin.sin_port = htons ( TFTP_PORT );
	udp_connect ( &tftp.udp, &server.st );
	tftp.filename = filename;
	tftp.buffer = &buffer;
	aop = tftp_get ( &tftp );
#else
	server.sin.sin_port = htons ( HTTP_PORT );
	memcpy ( &http.server, &server, sizeof ( http.server ) );
	http.hostname = inet_ntoa ( server.sin.sin_addr );
	http.filename = filename;
	http.buffer = &buffer;
	aop = http_get ( &http );
#endif

	if ( ( rc = async_wait ( aop ) ) != 0 ) {
		efree ( buffer.addr );
		return rc;
	}

	/* Fill in buffer address and length */
	*data = buffer.addr;
	*len = buffer.fill;

	return 0;
}
