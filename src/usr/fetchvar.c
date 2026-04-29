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
 */

FILE_LICENCE ( GPL2_OR_LATER );
FILE_SECBOOT ( PERMITTED );

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ipxe/xfer.h>
#include <ipxe/open.h>
#include <ipxe/iobuf.h>
#include <ipxe/xferbuf.h>
#include <ipxe/uri.h>
#include <ipxe/monojob.h>
#include <ipxe/settings.h>
#include <usr/fetchvar.h>

/** @file
 *
 * Fetch URI to setting
 *
 */

/** Maximum response size (8 kB) */
#define FETCHVAR_MAX_LEN 8192

/** A fetch-to-variable request */
struct fetchvar_request {
	/** Reference count for this object */
	struct refcnt refcnt;

	/** Job control interface */
	struct interface job;
	/** Data transfer interface */
	struct interface xfer;

	/** Data transfer buffer */
	struct xfer_buffer buffer;

	/** Setting name */
	char *setting_name;
};

/**
 * Free fetch-to-variable request
 *
 * @v refcnt		Reference counter
 */
static void fetchvar_free ( struct refcnt *refcnt ) {
	struct fetchvar_request *fetchvar_req =
		container_of ( refcnt, struct fetchvar_request, refcnt );

	xferbuf_free ( &fetchvar_req->buffer );
	free ( fetchvar_req );
}

/**
 * Terminate fetch-to-variable request
 *
 * @v fetchvar_req	Fetch-to-variable request
 * @v rc		Reason for termination
 */
static void fetchvar_close ( struct fetchvar_request *fetchvar_req, int rc ) {
	struct xfer_buffer *buffer = &fetchvar_req->buffer;
	struct settings *settings;
	struct setting setting;
	char *value;

	/* Store response in setting on success */
	if ( rc == 0 ) {

		/* Check size limit */
		if ( buffer->len > FETCHVAR_MAX_LEN ) {
			rc = -ENOSPC;
			goto done;
		}

		/* Allocate NUL-terminated copy of response */
		value = zalloc ( buffer->len + 1 /* NUL */ );
		if ( ! value ) {
			rc = -ENOMEM;
			goto done;
		}
		memcpy ( value, buffer->data, buffer->len );

		/* Parse setting name */
		if ( ( rc = parse_setting_name (
				fetchvar_req->setting_name,
				autovivify_child_settings,
				&settings, &setting ) ) != 0 ) {
			free ( value );
			goto done;
		}

		/* Apply default type if necessary */
		if ( ! setting.type )
			setting.type = &setting_type_string;

		/* Store value */
		rc = storef_setting ( settings, &setting, value );
		free ( value );
	}

 done:
	/* Shut down interfaces */
	intf_shutdown ( &fetchvar_req->xfer, rc );
	intf_shutdown ( &fetchvar_req->job, rc );
}

/**
 * Handle received data
 *
 * @v fetchvar_req	Fetch-to-variable request
 * @v iobuf		Datagram I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 */
static int fetchvar_deliver ( struct fetchvar_request *fetchvar_req,
			      struct io_buffer *iobuf,
			      struct xfer_metadata *meta ) {
	int rc;

	/* Add data to buffer */
	if ( ( rc = xferbuf_deliver ( &fetchvar_req->buffer,
				      iob_disown ( iobuf ), meta ) ) != 0 )
		goto err;

	return 0;

 err:
	fetchvar_close ( fetchvar_req, rc );
	return rc;
}

/**
 * Get underlying data transfer buffer
 *
 * @v fetchvar_req	Fetch-to-variable request
 * @ret xferbuf		Data transfer buffer, or NULL on error
 */
static struct xfer_buffer *
fetchvar_buffer ( struct fetchvar_request *fetchvar_req ) {

	return &fetchvar_req->buffer;
}

/**
 * Redirect data transfer interface
 *
 * @v fetchvar_req	Fetch-to-variable request
 * @v type		New location type
 * @v args		Remaining arguments depend upon location type
 * @ret rc		Return status code
 */
static int fetchvar_vredirect ( struct fetchvar_request *fetchvar_req,
				int type, va_list args ) {
	int rc;

	/* Redirect to new location */
	if ( ( rc = xfer_vreopen ( &fetchvar_req->xfer, type, args ) ) != 0 )
		goto err;

	return 0;

 err:
	fetchvar_close ( fetchvar_req, rc );
	return rc;
}

/** Fetch-to-variable data transfer interface operations */
static struct interface_operation fetchvar_xfer_operations[] = {
	INTF_OP ( xfer_deliver, struct fetchvar_request *, fetchvar_deliver ),
	INTF_OP ( xfer_buffer, struct fetchvar_request *, fetchvar_buffer ),
	INTF_OP ( xfer_vredirect, struct fetchvar_request *,
		  fetchvar_vredirect ),
	INTF_OP ( intf_close, struct fetchvar_request *, fetchvar_close ),
};

/** Fetch-to-variable data transfer interface descriptor */
static struct interface_descriptor fetchvar_xfer_desc =
	INTF_DESC ( struct fetchvar_request, xfer, fetchvar_xfer_operations );

/** Fetch-to-variable job control interface operations */
static struct interface_operation fetchvar_job_operations[] = {
	INTF_OP ( intf_close, struct fetchvar_request *, fetchvar_close ),
};

/** Fetch-to-variable job control interface descriptor */
static struct interface_descriptor fetchvar_job_desc =
	INTF_DESC ( struct fetchvar_request, job, fetchvar_job_operations );

/**
 * Fetch URI and store response body in setting
 *
 * @v uri_string	URI string
 * @v setting_name	Setting name
 * @ret rc		Return status code
 */
int fetchvar ( const char *uri_string, const char *setting_name ) {
	struct fetchvar_request *fetchvar_req;
	struct uri *raw_uri;
	struct uri *uri;
	int rc;

	/* Parse URI */
	raw_uri = parse_uri ( uri_string );
	if ( ! raw_uri ) {
		rc = -ENOMEM;
		goto err_parse_uri;
	}

	/* Resolve URI */
	uri = resolve_uri ( cwuri, raw_uri );
	if ( ! uri ) {
		rc = -ENOMEM;
		goto err_resolve_uri;
	}

	/* Allocate and initialise structure */
	fetchvar_req = zalloc ( sizeof ( *fetchvar_req ) +
				strlen ( setting_name ) + 1 /* NUL */ );
	if ( ! fetchvar_req ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	ref_init ( &fetchvar_req->refcnt, fetchvar_free );
	intf_init ( &fetchvar_req->job, &fetchvar_job_desc,
		    &fetchvar_req->refcnt );
	intf_init ( &fetchvar_req->xfer, &fetchvar_xfer_desc,
		    &fetchvar_req->refcnt );
	xferbuf_malloc_init ( &fetchvar_req->buffer );
	fetchvar_req->setting_name = ( ( void * ) ( fetchvar_req + 1 ) );
	strcpy ( fetchvar_req->setting_name, setting_name );

	/* Open URI */
	if ( ( rc = xfer_open_uri ( &fetchvar_req->xfer, uri ) ) != 0 )
		goto err_open;

	/* Attach parent interface, mortalise self, and return */
	intf_plug_plug ( &fetchvar_req->job, &monojob );
	ref_put ( &fetchvar_req->refcnt );
	uri_put ( uri );
	uri_put ( raw_uri );

	/* Wait for completion */
	if ( ( rc = monojob_wait ( uri_string, 0 ) ) != 0 ) {
		printf ( "Could not fetch %s: %s\n",
			 uri_string, strerror ( rc ) );
		return rc;
	}

	return 0;

 err_open:
	fetchvar_close ( fetchvar_req, rc );
	ref_put ( &fetchvar_req->refcnt );
 err_alloc:
	uri_put ( uri );
 err_resolve_uri:
	uri_put ( raw_uri );
 err_parse_uri:
	return rc;
}
