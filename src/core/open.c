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

#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <gpxe/xfer.h>
#include <gpxe/uri.h>
#include <gpxe/socket.h>
#include <gpxe/open.h>

/** @file
 *
 * Data transfer interface opening
 *
 */

/** Registered URI openers */
static struct uri_opener uri_openers[0]
	__table_start ( struct uri_opener, uri_openers );
static struct uri_opener uri_openers_end[0]
	__table_end ( struct uri_opener, uri_openers );

/** Registered socket openers */
static struct socket_opener socket_openers[0]
	__table_start ( struct socket_opener, socket_openers );
static struct socket_opener socket_openers_end[0]
	__table_end ( struct socket_opener, socket_openers );

/**
 * Open URI
 *
 * @v xfer		Data transfer interface
 * @v uri_string	URI string (e.g. "http://etherboot.org/kernel")
 * @ret rc		Return status code
 */
int xfer_open_uri ( struct xfer_interface *xfer, const char *uri_string ) {
	struct uri *uri;
	struct uri_opener *opener;

	DBGC ( xfer, "XFER %p opening URI %s\n", xfer, uri_string );

	uri = parse_uri ( uri_string );
	if ( ! uri )
		return -ENOMEM;

	for ( opener = uri_openers ; opener < uri_openers_end ; opener++ ) {
		if ( strcmp ( uri->scheme, opener->scheme ) == 0 ) {
			return opener->open ( xfer, uri );
		}
	}

	DBGC ( xfer, "XFER %p attempted to open unsupported URI scheme "
	       "\"%s\"\n", xfer, uri->scheme );
	free_uri ( uri );
	return -ENOTSUP;
}

/**
 * Open socket
 *
 * @v xfer		Data transfer interface
 * @v domain		Communication domain (e.g. PF_INET)
 * @v type		Communication semantics (e.g. SOCK_STREAM)
 * @v peer		Peer socket address
 * @v local		Local socket address, or NULL
 * @ret rc		Return status code
 */
int xfer_open_socket ( struct xfer_interface *xfer,
		       int domain, int type, struct sockaddr *peer,
		       struct sockaddr *local ) {
	struct socket_opener *opener;

	DBGC ( xfer, "XFER %p opening (%s,%s) socket\n", xfer,
	       socket_domain_name ( domain ), socket_type_name ( type ) );

	for ( opener = socket_openers; opener < socket_openers_end; opener++ ){
		if ( ( opener->domain == domain ) &&
		     ( opener->type == type ) ) {
			return opener->open ( xfer, peer, local );
		}
	}

	DBGC ( xfer, "XFER %p attempted to open unsupported socket type "
	       "(%s,%s)\n", xfer, socket_domain_name ( domain ),
	       socket_type_name ( type ) );
	return -ENOTSUP;
}

/**
 * Open location
 *
 * @v xfer		Data transfer interface
 * @v type		Location type
 * @v args		Remaining arguments depend upon location type
 * @ret rc		Return status code
 */
int xfer_vopen ( struct xfer_interface *xfer, int type, va_list args ) {
	switch ( type ) {
	case LOCATION_URI: {
		const char *uri_string = va_arg ( args, const char * );

		return xfer_open_uri ( xfer, uri_string ); }
	case LOCATION_SOCKET: {
		int domain = va_arg ( args, int );
		int type = va_arg ( args, int );
		struct sockaddr *peer = va_arg ( args, struct sockaddr * );
		struct sockaddr *local = va_arg ( args, struct sockaddr * );

		return xfer_open_socket ( xfer, domain, type, peer, local ); }
	default:
		DBGC ( xfer, "XFER %p attempted to open unsupported location "
		       "type %d\n", xfer, type );
		return -ENOTSUP;
	}
}

/**
 * Open location
 *
 * @v xfer		Data transfer interface
 * @v type		Location type
 * @v ...		Remaining arguments depend upon location type
 * @ret rc		Return status code
 */
int xfer_open ( struct xfer_interface *xfer, int type, ... ) {
	va_list args;
	int rc;

	va_start ( args, type );
	rc = xfer_vopen ( xfer, type, args );
	va_end ( args );
	return rc;
}
