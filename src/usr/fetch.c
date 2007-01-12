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
#include <gpxe/tftp.h>
#include <gpxe/dhcp.h>

/**
 * Fetch file as executable/loadable image
 *
 * @v image		Executable/loadable image
 * @v filename		Filename
 * @ret rc		Return status code
 */
int fetch ( struct image *image, const char *filename ) {
	struct buffer buffer;
	int rc;

	/* Allocate an expandable buffer to hold the file */
	if ( ( rc = ebuffer_alloc ( &buffer, 0 ) ) != 0 )
		return rc;

	/* Retrieve the file */
	struct tftp_session tftp;
	union {
		struct sockaddr_tcpip st;
		struct sockaddr_in sin;
	} server;

	memset ( &tftp, 0, sizeof ( tftp ) );
	memset ( &server, 0, sizeof ( server ) );
	server.sin.sin_family = AF_INET;
	find_global_dhcp_ipv4_option ( DHCP_EB_SIADDR,
				       &server.sin.sin_addr );
	server.sin.sin_port = htons ( TFTP_PORT );
	udp_connect ( &tftp.udp, &server.st );
	tftp.filename = filename;
	tftp.buffer = &buffer;
	if ( ( rc = async_wait ( tftp_get ( &tftp ) ) ) != 0 ) {
		efree ( buffer.addr );
		return rc;
	}

	/* Transfer ownserhip of the data buffer to the image */
	image->data = buffer.addr;
	image->len = buffer.fill;
	image->free = efree;

	return 0;
}
