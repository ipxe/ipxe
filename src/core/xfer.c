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
 * Deliver datagram
 *
 * @v xfer		Data-transfer interface
 * @v iobuf		Datagram I/O buffer
 * @ret rc		Return status code
 */
int deliver ( struct xfer_interface *xfer, struct io_buffer *iobuf ) {
	struct xfer_interface *dest = xfer_dest ( xfer );

	return dest->op->deliver ( dest, xfer, iobuf );
}

/**
 * Deliver datagram as raw data
 *
 * @v xfer		Data-transfer interface
 * @v src		Source interface
 * @v iobuf		Datagram I/O buffer
 * @ret rc		Return status code
 *
 * This function is intended to be used as the deliver() method for
 * data transfer interfaces that prefer to handle raw data.
 */
int deliver_as_raw ( struct xfer_interface *xfer,
		     struct xfer_interface *src,
		     struct io_buffer *iobuf ) {
	int rc;

	rc = xfer->op->deliver_raw ( xfer, src, iobuf->data,
				     iob_len ( iobuf ) );
	free_iob ( iobuf );
	return rc;
}

/**
 * Deliver datagram as I/O buffer
 *
 * @v xfer		Data-transfer interface
 * @v src		Source interface
 * @v data		Data buffer
 * @v len		Length of data buffer
 * @ret rc		Return status code
 *
 * This function is intended to be used as the deliver_raw() method
 * for data transfer interfaces that prefer to handle I/O buffers.
 */
int deliver_as_iobuf ( struct xfer_interface *xfer,
		       struct xfer_interface *src,
		       const void *data, size_t len ) {
	struct io_buffer *iobuf;

#warning "Do we need interface-specific I/O buffer allocation?"
	iobuf = alloc_iob ( len );
	if ( ! iobuf )
		return -ENOMEM;

	memcpy ( iob_put ( iobuf, len ), data, len );
	return xfer->op->deliver ( xfer, src, iobuf );
}

/**
 * Null deliver datagram as raw data
 *
 * @v xfer		Data-transfer interface
 * @v src		Source interface
 * @v data		Data buffer
 * @v len		Length of data buffer
 * @ret rc		Return status code
 */
static int null_deliver_raw ( struct xfer_interface *xfer,
			      struct xfer_interface *src,
			      const void *data __unused, size_t len ) {
	DBGC ( src, "XFER %p->%p %zd bytes delivered %s\n", src, xfer, len,
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
