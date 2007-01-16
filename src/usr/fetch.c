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

#include <errno.h>
#include <vsprintf.h>
#include <gpxe/umalloc.h>
#include <gpxe/ebuffer.h>
#include <gpxe/image.h>
#include <gpxe/uri.h>
#include <usr/fetch.h>

#include <byteswap.h>
#include <gpxe/dhcp.h>
#include <gpxe/tftp.h>
#include <gpxe/http.h>
#include <gpxe/ftp.h>

/**
 * Fetch file
 *
 * @v filename		Filename to fetch
 * @ret data		Loaded file
 * @ret len		Length of loaded file
 * @ret rc		Return status code
 *
 * Fetch file to an external buffer allocated with umalloc().  The
 * caller is responsible for eventually freeing the buffer with
 * ufree().
 */
int fetch ( const char *uri_string, userptr_t *data, size_t *len ) {
	struct uri *uri;
	struct buffer buffer;
	int rc;

	/* Parse the URI */
	uri = parse_uri ( uri_string );
	if ( ! uri ) {
		rc = -ENOMEM;
		goto err_parse_uri;
	}

	/* Allocate an expandable buffer to hold the file */
	if ( ( rc = ebuffer_alloc ( &buffer, 0 ) ) != 0 ) {
		goto err_ebuffer_alloc;
	}

#warning "Temporary pseudo-URL parsing code"

	/* Retrieve the file */
	struct async async;

	int ( * download ) ( struct uri *uri, struct buffer *buffer,
			     struct async *parent );

	if ( ! uri->scheme ) {
		download = tftp_get;
	} else {
		if ( strcmp ( uri->scheme, "http" ) == 0 ) {
			download = http_get;
		} else if ( strcmp ( uri->scheme, "ftp" ) == 0 ) {
			download = ftp_get;
		} else {
			download = tftp_get;
		}
	}

	if ( ( rc = async_block ( &async,
				  download ( uri, &buffer, &async ) ) )  != 0 )
		goto err;

	/* Fill in buffer address and length */
	*data = buffer.addr;
	*len = buffer.fill;

	/* Release temporary resources.  The ebuffer storage is now
	 * owned by our caller, so we don't free it.
	 */
	free_uri ( uri );
	return 0;

 err:
	ufree ( buffer.addr );
 err_ebuffer_alloc:
	free_uri ( uri );
 err_parse_uri:
	return rc;
}
