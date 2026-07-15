/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
FILE_SECBOOT ( PERMITTED );

#include <string.h>
#include <errno.h>
#include <ipxe/malloc.h>
#include <ipxe/xfer.h>
#include <ipxe/process.h>
#include <ipxe/blob.h>

/** @file
 *
 * Openable data blobs
 *
 * Within the iPXE data transfer interface model, openers are fully
 * asynchronous and may not deliver any data until after the opener
 * has returned.  An openable data blob provides a way to open an
 * interface that will then simply return a single fixed blob of data.
 */

/** An openable data blob */
struct blob {
	/** Reference count */
	struct refcnt refcnt;
	/** Data transfer interface */
	struct interface xfer;
	/** Download process */
	struct process process;
	/** Data */
	const void *data;
	/** Length of data */
	size_t len;
};

/**
 * Close data blob
 *
 * @v blob		Data blob
 * @v rc		Reason for close
 */
static void blob_close ( struct blob *blob, int rc ) {

	/* Stop process */
	process_del ( &blob->process );

	/* Shut down interface */
	intf_shutdown ( &blob->xfer, rc );
}

/**
 * Process data blob
 *
 * @v blob		Data blob
 */
static void blob_step ( struct blob *blob ) {
	int rc;

	/* Deliver data when window opens */
	if ( xfer_window ( &blob->xfer ) ) {
		rc = xfer_deliver_raw ( &blob->xfer, blob->data, blob->len );
		blob_close ( blob, rc );
	}
}

/** Data blob interface operations */
static struct interface_operation blob_xfer_op[] = {
	INTF_OP ( xfer_window_changed, struct blob *, blob_step ),
	INTF_OP ( intf_close, struct blob *, blob_close ),
};

/** Data blob interface descriptor */
static struct interface_descriptor blob_xfer_desc =
	INTF_DESC ( struct blob, xfer, blob_xfer_op );

/** Data blob process descriptor */
static struct process_descriptor blob_process_desc =
	PROC_DESC_ONCE ( struct blob, process, blob_step );

/**
 * Open data blob
 *
 * @v xfer		Data transfer interface
 * @v data		Data
 * @v len		Length of data
 * @ret rc		Return status code
 */
int blob_open ( struct interface *xfer, const void *data, size_t len ) {
	struct blob *blob;
	void *copy;

	/* Allocate and initialise structure */
	blob = zalloc ( sizeof ( *blob ) + len );
	if ( ! blob )
		return -ENOMEM;
	ref_init ( &blob->refcnt, NULL );
	intf_init ( &blob->xfer, &blob_xfer_desc, &blob->refcnt );
	process_init ( &blob->process, &blob_process_desc, &blob->refcnt );
	copy = ( ( ( void * ) blob ) + sizeof ( *blob ) );
	blob->data = copy;
	blob->len = len;
	memcpy ( copy, data, len );

	/* Attach parent interface, mortalise self, and return */
	intf_plug_plug ( &blob->xfer, xfer );
	ref_put ( &blob->refcnt );
	return 0;
}
