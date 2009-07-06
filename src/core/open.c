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

FILE_LICENCE ( GPL2_OR_LATER );

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

/**
 * Open URI
 *
 * @v xfer		Data transfer interface
 * @v uri		URI
 * @ret rc		Return status code
 *
 * The URI will be regarded as being relative to the current working
 * URI (see churi()).
 */
int xfer_open_uri ( struct xfer_interface *xfer, struct uri *uri ) {
	struct uri_opener *opener;
	struct uri *resolved_uri;
	int rc = -ENOTSUP;

	/* Resolve URI */
	resolved_uri = resolve_uri ( cwuri, uri );
	if ( ! resolved_uri )
		return -ENOMEM;

	/* Find opener which supports this URI scheme */
	for_each_table_entry ( opener, URI_OPENERS ) {
		if ( strcmp ( resolved_uri->scheme, opener->scheme ) == 0 ) {
			DBGC ( xfer, "XFER %p opening %s URI\n",
			       xfer, opener->scheme );
			rc = opener->open ( xfer, resolved_uri );
			goto done;
		}
	}
	DBGC ( xfer, "XFER %p attempted to open unsupported URI scheme "
	       "\"%s\"\n", xfer, resolved_uri->scheme );

 done:
	uri_put ( resolved_uri );
	return rc;
}

/**
 * Open URI string
 *
 * @v xfer		Data transfer interface
 * @v uri_string	URI string (e.g. "http://etherboot.org/kernel")
 * @ret rc		Return status code
 *
 * The URI will be regarded as being relative to the current working
 * URI (see churi()).
 */
int xfer_open_uri_string ( struct xfer_interface *xfer,
			   const char *uri_string ) {
	struct uri *uri;
	int rc;

	DBGC ( xfer, "XFER %p opening URI %s\n", xfer, uri_string );

	uri = parse_uri ( uri_string );
	if ( ! uri )
		return -ENOMEM;

	rc = xfer_open_uri ( xfer, uri );

	uri_put ( uri );
	return rc;
}

/**
 * Open socket
 *
 * @v xfer		Data transfer interface
 * @v semantics		Communication semantics (e.g. SOCK_STREAM)
 * @v peer		Peer socket address
 * @v local		Local socket address, or NULL
 * @ret rc		Return status code
 */
int xfer_open_socket ( struct xfer_interface *xfer, int semantics,
		       struct sockaddr *peer, struct sockaddr *local ) {
	struct socket_opener *opener;

	DBGC ( xfer, "XFER %p opening (%s,%s) socket\n", xfer,
	       socket_semantics_name ( semantics ),
	       socket_family_name ( peer->sa_family ) );

	for_each_table_entry ( opener, SOCKET_OPENERS ) {
		if ( ( opener->semantics == semantics ) &&
		     ( opener->family == peer->sa_family ) ) {
			return opener->open ( xfer, peer, local );
		}
	}

	DBGC ( xfer, "XFER %p attempted to open unsupported socket type "
	       "(%s,%s)\n", xfer, socket_semantics_name ( semantics ),
	       socket_family_name ( peer->sa_family ) );
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
	case LOCATION_URI_STRING: {
		const char *uri_string = va_arg ( args, const char * );

		return xfer_open_uri_string ( xfer, uri_string ); }
	case LOCATION_URI: {
		struct uri *uri = va_arg ( args, struct uri * );

		return xfer_open_uri ( xfer, uri ); }
	case LOCATION_SOCKET: {
		int semantics = va_arg ( args, int );
		struct sockaddr *peer = va_arg ( args, struct sockaddr * );
		struct sockaddr *local = va_arg ( args, struct sockaddr * );

		return xfer_open_socket ( xfer, semantics, peer, local ); }
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

/**
 * Reopen location
 *
 * @v xfer		Data transfer interface
 * @v type		Location type
 * @v args		Remaining arguments depend upon location type
 * @ret rc		Return status code
 *
 * This will close the existing connection and open a new connection
 * using xfer_vopen().  It is intended to be used as a .vredirect
 * method handler.
 */
int xfer_vreopen ( struct xfer_interface *xfer, int type, va_list args ) {

	/* Close existing connection */
	xfer_close ( xfer, 0 );

	/* Open new location */
	return xfer_vopen ( xfer, type, args );
}
