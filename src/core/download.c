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
 * Download protocols
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <gpxe/umalloc.h>
#include <gpxe/ebuffer.h>
#include <gpxe/download.h>

static struct async_operations download_async_operations;

/** Registered download protocols */
static struct download_protocol download_protocols[0]
	__table_start ( struct download_protocol, download_protocols );
static struct download_protocol download_protocols_end[0]
	__table_end ( struct download_protocol, download_protocols );

/**
 * Identify download protocol
 *
 * @v name		Download protocol name
 * @ret protocol	Download protocol, or NULL
 */
static struct download_protocol * find_protocol ( const char *name ) {
	struct download_protocol *protocol;

	for ( protocol = download_protocols; protocol < download_protocols_end;
	      protocol++ ) {
		if ( strcmp ( name, protocol->name ) == 0 )
			return protocol;
	}
	return NULL;
}

/**
 * Start download
 *
 * @v uri_string	URI as a string (e.g. "http://www.nowhere.com/vmlinuz")
 * @v parent		Parent asynchronous operation
 * @ret data		Loaded file
 * @ret len		Length of loaded file
 * @ret rc		Return status code
 *
 * Starts download of a file to a user buffer.  The user buffer is
 * allocated with umalloc().  The parent asynchronous operation will
 * be notified via SIGCHLD when the download completes.  If the
 * download completes successfully, the @c data and @c len fields will
 * have been filled in, and the parent takes ownership of the buffer,
 * which must eventually be freed with ufree().
 *
 * The uri_string does not need to remain persistent for the duration
 * of the download; the parent may discard it as soon as
 * start_download returns.
 */
int start_download ( const char *uri_string, struct async *parent,
		     userptr_t *data, size_t *len ) {
	struct download *download;
	int rc;

	/* Allocate and populate download structure */
	download = malloc ( sizeof ( *download ) );
	if ( ! download )
		return -ENOMEM;
	memset ( download, 0, sizeof ( *download ) );
	download->data = data;
	download->len = len;
	async_init ( &download->async, &download_async_operations, parent );

	/* Parse the URI */
	download->uri = parse_uri ( uri_string );
	if ( ! download->uri ) {
		rc = -ENOMEM;
		goto err;
	}

	/* Allocate an expandable buffer to hold the file */
	if ( ( rc = ebuffer_alloc ( &download->buffer, 0 ) ) != 0 )
		goto err;

	/* Identify the download protocol */
	download->protocol = find_protocol ( download->uri->scheme );
	if ( ! download->protocol ) {
		DBG ( "No such protocol \"%s\"\n", download->uri->scheme );
		rc = -ENOTSUP;
		goto err;
	}

	/* Start the actual download */
	if ( ( rc = download->protocol->start_download ( download->uri,
			       &download->buffer, &download->async ) ) != 0 ) {
		DBG ( "Could not start \"%s\" download: %s\n",
		      download->uri->scheme, strerror ( rc ) );
		goto err;
	}

	return 0;

 err:
	async_uninit ( &download->async );
	ufree ( download->buffer.addr );
	free_uri ( download->uri );
	free ( download );
	return rc;
}

/**
 * Handle download termination
 *
 * @v async		Download asynchronous operation
 * @v signal		SIGCHLD
 */
static void download_sigchld ( struct async *async,
			       enum signal signal __unused ) {
	struct download *download =
		container_of ( async, struct download, async );
	int rc;

	/* Reap child */
	async_wait ( async, &rc, 1 );

	/* Clean up */
	if ( rc == 0 ) {
		/* Transfer ownership of buffer to parent */
		*(download->data) = download->buffer.addr;
		*(download->len) = download->buffer.fill;
	} else {
		/* Discard the buffer */
		ufree ( download->buffer.addr );
	}
	free_uri ( download->uri );
	download->uri = NULL;

	/* Terminate ourselves */
	async_done ( async, rc );
}

/**
 * Free download resources
 *
 * @v async		Download asynchronous operation
 */
static void download_reap ( struct async *async ) {
	free ( container_of ( async, struct download, async ) );
}

/** Download asynchronous operations */
static struct async_operations download_async_operations = {
	.reap = download_reap,
	.signal = {
		[SIGCHLD] = download_sigchld,
	},
};
