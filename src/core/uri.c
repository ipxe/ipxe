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

/** @file
 *
 * Uniform Resource Identifiers
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <gpxe/uri.h>

/**
 * Parse URI
 *
 * @v uri_string	URI as a string
 * @ret uri		URI
 *
 * Splits a URI into its component parts.  The return URI structure is
 * dynamically allocated and must eventually be freed by calling
 * uri_put().
 */
struct uri * parse_uri ( const char *uri_string ) {
	struct uri *uri;
	char *raw;
	char *tmp;
	char *path = NULL;
	char *authority = NULL;
	size_t raw_len;

	/* Allocate space for URI struct and a copy of the string */
	raw_len = ( strlen ( uri_string ) + 1 /* NUL */ );
	uri = malloc ( sizeof ( *uri ) + raw_len );
	if ( ! uri )
		return NULL;
	raw = ( ( ( char * ) uri ) + sizeof ( *uri ) );

	/* Zero URI struct and copy in the raw string */
	memset ( uri, 0, sizeof ( *uri ) );
	memcpy ( raw, uri_string, raw_len );

	/* Start by chopping off the fragment, if it exists */
	if ( ( tmp = strchr ( raw, '#' ) ) ) {
		*(tmp++) = '\0';
		uri->fragment = tmp;
	}

	/* Identify absolute/relative URI */
	if ( ( tmp = strchr ( raw, ':' ) ) ) {
		/* Absolute URI: identify hierarchical/opaque */
		uri->scheme = raw;
		*(tmp++) = '\0';
		if ( *tmp == '/' ) {
			/* Absolute URI with hierarchical part */
			path = tmp;
		} else {
			/* Absolute URI with opaque part */
			uri->opaque = tmp;
		}
	} else {
		/* Relative URI */
		path = raw;
	}

	/* If we don't have a path (i.e. we have an absolute URI with
	 * an opaque portion, we're already finished processing
	 */
	if ( ! path )
		goto done;

	/* Chop off the query, if it exists */
	if ( ( tmp = strchr ( path, '?' ) ) ) {
		*(tmp++) = '\0';
		uri->query = tmp;
	}

	/* Identify net/absolute/relative path */
	if ( strncmp ( path, "//", 2 ) == 0 ) {
		/* Net path.  If this is terminated by the first '/'
		 * of an absolute path, then we have no space for a
		 * terminator after the authority field, so shuffle
		 * the authority down by one byte, overwriting one of
		 * the two slashes.
		 */
		authority = ( path + 2 );
		if ( ( tmp = strchr ( authority, '/' ) ) ) {
			/* Shuffle down */
			uri->path = tmp;
			memmove ( ( authority - 1 ), authority,
				  ( tmp - authority ) );
			authority--;
			*(--tmp) = '\0';
		}
	} else {
		/* Absolute/relative path */
		uri->path = path;
	}

	/* Split authority into user[:password] and host[:port] portions */
	if ( ( tmp = strchr ( authority, '@' ) ) ) {
		/* Has user[:password] */
		*(tmp++) = '\0';
		uri->host = tmp;
		uri->user = authority;
		if ( ( tmp = strchr ( authority, ':' ) ) ) {
			/* Has password */
			*(tmp++) = '\0';
			uri->password = tmp;
		}
	} else {
		/* No user:password */
		uri->host = authority;
	}

	/* Split host into host[:port] */
	if ( ( tmp = strchr ( uri->host, ':' ) ) ) {
		*(tmp++) = '\0';
		uri->port = tmp;
	}

 done:
	DBG ( "URI \"%s\" split into", raw );
	if ( uri->scheme )
		DBG ( " scheme \"%s\"", uri->scheme );
	if ( uri->opaque )
		DBG ( " opaque \"%s\"", uri->opaque );
	if ( uri->user )
		DBG ( " user \"%s\"", uri->user );
	if ( uri->password )
		DBG ( " password \"%s\"", uri->password );
	if ( uri->host )
		DBG ( " host \"%s\"", uri->host );
	if ( uri->port )
		DBG ( " port \"%s\"", uri->port );
	if ( uri->path )
		DBG ( " path \"%s\"", uri->path );
	if ( uri->query )
		DBG ( " query \"%s\"", uri->query );
	if ( uri->fragment )
		DBG ( " fragment \"%s\"", uri->fragment );
	DBG ( "\n" );

	return uri;
}

/**
 * Get port from URI
 *
 * @v uri		URI
 * @v default_port	Default port to use if none specified in URI
 * @ret port		Port
 */
unsigned int uri_port ( struct uri *uri, unsigned int default_port ) {
	return ( uri->port ? strtoul ( uri->port, NULL, 0 ) : default_port );
}
