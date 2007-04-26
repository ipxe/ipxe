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
 * Null deliver datagram
 *
 * @v xfer		Data-transfer interface
 * @v src		Source interface
 * @v iobuf		Datagram I/O buffer
 * @ret rc		Return status code
 */
static int null_deliver ( struct xfer_interface *xfer __unused,
			  struct xfer_interface *src __unused,
			  struct io_buffer *iobuf ) {
	free_iob ( iobuf );
	return -EPIPE;
}

/** Null data transfer interface operations */
struct xfer_interface_operations null_xfer_ops = {
	.deliver = null_deliver,
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
