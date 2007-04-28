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

#include <string.h>
#include <errno.h>
#include <gpxe/xfer.h>

/** @file
 *
 * Data transfer interfaces
 *
 */

/**
 * Send redirection event
 *
 * @v xfer		Data-transfer interface
 * @v type		New location type
 * @v args		Remaining arguments depend upon location type
 * @ret rc		Return status code
 */
int vredirect ( struct xfer_interface *xfer, int type, va_list args ) {
	struct xfer_interface *dest = xfer_dest ( xfer );

	return dest->op->vredirect ( dest, type, args );
}

/**
 * Send redirection event
 *
 * @v xfer		Data-transfer interface
 * @v type		New location type
 * @v ...		Remaining arguments depend upon location type
 * @ret rc		Return status code
 */
int redirect ( struct xfer_interface *xfer, int type, ... ) {
	va_list args;
	int rc;

	va_start ( args, type );
	rc = vredirect ( xfer, type, args );
	va_end ( args );
	return rc;
}

/**
 * Deliver datagram
 *
 * @v xfer		Data-transfer interface
 * @v iobuf		Datagram I/O buffer
 * @ret rc		Return status code
 */
int deliver ( struct xfer_interface *xfer, struct io_buffer *iobuf ) {
	struct xfer_interface *dest = xfer_dest ( xfer );

	return dest->op->deliver ( dest, iobuf );
}

/**
 * Deliver datagram as raw data
 *
 * @v xfer		Data-transfer interface
 * @v iobuf		Datagram I/O buffer
 * @ret rc		Return status code
 */
int deliver_raw ( struct xfer_interface *xfer, const void *data, size_t len ) {
	struct xfer_interface *dest = xfer_dest ( xfer );

	return dest->op->deliver_raw ( dest, data, len );
}

/****************************************************************************
 *
 * Helper methods
 *
 * These functions are designed to be used as methods in the
 * xfer_interface_operations table.
 *
 */

/**
 * Deliver datagram as raw data
 *
 * @v xfer		Data-transfer interface
 * @v iobuf		Datagram I/O buffer
 * @ret rc		Return status code
 *
 * This function is intended to be used as the deliver() method for
 * data transfer interfaces that prefer to handle raw data.
 */
int deliver_as_raw ( struct xfer_interface *xfer,
		     struct io_buffer *iobuf ) {
	int rc;

	rc = xfer->op->deliver_raw ( xfer, iobuf->data, iob_len ( iobuf ) );
	free_iob ( iobuf );
	return rc;
}

/**
 * Deliver datagram as I/O buffer
 *
 * @v xfer		Data-transfer interface
 * @v data		Data buffer
 * @v len		Length of data buffer
 * @ret rc		Return status code
 *
 * This function is intended to be used as the deliver_raw() method
 * for data transfer interfaces that prefer to handle I/O buffers.
 */
int deliver_as_iobuf ( struct xfer_interface *xfer,
		       const void *data, size_t len ) {
	struct io_buffer *iobuf;

	iobuf = alloc_iob ( len );
	if ( ! iobuf )
		return -ENOMEM;

	memcpy ( iob_put ( iobuf, len ), data, len );
	return xfer->op->deliver ( xfer, iobuf );
}

/****************************************************************************
 *
 * Null data transfer interface
 *
 */

/**
 * Null deliver datagram as raw data
 *
 * @v xfer		Data-transfer interface
 * @v data		Data buffer
 * @v len		Length of data buffer
 * @ret rc		Return status code
 */
static int null_deliver_raw ( struct xfer_interface *xfer,
			      const void *data __unused, size_t len ) {
	DBGC ( xfer, "XFER %p %zd bytes delivered %s\n", xfer, len,
	       ( ( xfer == &null_xfer ) ?
		 "before connection" : "after termination" ) );
	return -EPIPE;
}

/** Null data transfer interface operations */
struct xfer_interface_operations null_xfer_ops = {
	.deliver	= deliver_as_raw,
	.deliver_raw	= null_deliver_raw,
};

/**
 * Null data transfer interface
 *
 * This is the interface to which data transfer interfaces are
 * connected when unplugged.  It will never generate messages, and
 * will silently absorb all received messages.
 */
struct xfer_interface null_xfer = {
	.intf = {
		.dest = &null_xfer.intf,
		.refcnt = null_refcnt,
	},
	.op = &null_xfer_ops,
};
