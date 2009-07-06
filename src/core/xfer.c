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

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <gpxe/xfer.h>

/** @file
 *
 * Data transfer interfaces
 *
 */

/**
 * Dummy transfer metadata
 *
 * This gets passed to xfer_interface::deliver_iob() and equivalents
 * when no metadata is available.
 */
static struct xfer_metadata dummy_metadata;

/**
 * Close data transfer interface
 *
 * @v xfer		Data transfer interface
 * @v rc		Reason for close
 */
void xfer_close ( struct xfer_interface *xfer, int rc ) {
	struct xfer_interface *dest = xfer_get_dest ( xfer );
	struct xfer_interface_operations *op = xfer->op;

	DBGC ( xfer, "XFER %p->%p close\n", xfer, dest );

	xfer_unplug ( xfer );
	xfer_nullify ( xfer );
	dest->op->close ( dest, rc );
	xfer->op = op;
	xfer_put ( dest );
}

/**
 * Send redirection event
 *
 * @v xfer		Data transfer interface
 * @v type		New location type
 * @v args		Remaining arguments depend upon location type
 * @ret rc		Return status code
 */
int xfer_vredirect ( struct xfer_interface *xfer, int type, va_list args ) {
	struct xfer_interface *dest = xfer_get_dest ( xfer );
	int rc;

	DBGC ( xfer, "XFER %p->%p redirect\n", xfer, dest );

	rc = dest->op->vredirect ( dest, type, args );

	if ( rc != 0 ) {
		DBGC ( xfer, "XFER %p<-%p redirect: %s\n", xfer, dest,
		       strerror ( rc ) );
	}
	xfer_put ( dest );
	return rc;
}

/**
 * Send redirection event
 *
 * @v xfer		Data transfer interface
 * @v type		New location type
 * @v ...		Remaining arguments depend upon location type
 * @ret rc		Return status code
 */
int xfer_redirect ( struct xfer_interface *xfer, int type, ... ) {
	va_list args;
	int rc;

	va_start ( args, type );
	rc = xfer_vredirect ( xfer, type, args );
	va_end ( args );
	return rc;
}

/**
 * Check flow control window
 *
 * @v xfer		Data transfer interface
 * @ret len		Length of window
 */
size_t xfer_window ( struct xfer_interface *xfer ) {
	struct xfer_interface *dest = xfer_get_dest ( xfer );
	size_t len;

	len = dest->op->window ( dest );

	xfer_put ( dest );
	return len;
}

/**
 * Allocate I/O buffer
 *
 * @v xfer		Data transfer interface
 * @v len		I/O buffer payload length
 * @ret iobuf		I/O buffer
 */
struct io_buffer * xfer_alloc_iob ( struct xfer_interface *xfer, size_t len ) {
	struct xfer_interface *dest = xfer_get_dest ( xfer );
	struct io_buffer *iobuf;

	DBGC ( xfer, "XFER %p->%p alloc_iob %zd\n", xfer, dest, len );

	iobuf = dest->op->alloc_iob ( dest, len );

	if ( ! iobuf ) {
		DBGC ( xfer, "XFER %p<-%p alloc_iob failed\n", xfer, dest );
	}
	xfer_put ( dest );
	return iobuf;
}

/**
 * Deliver datagram as I/O buffer with metadata
 *
 * @v xfer		Data transfer interface
 * @v iobuf		Datagram I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 */
int xfer_deliver_iob_meta ( struct xfer_interface *xfer,
			    struct io_buffer *iobuf,
			    struct xfer_metadata *meta ) {
	struct xfer_interface *dest = xfer_get_dest ( xfer );
	int rc;

	DBGC ( xfer, "XFER %p->%p deliver_iob %zd\n", xfer, dest,
	       iob_len ( iobuf ) );

	rc = dest->op->deliver_iob ( dest, iobuf, meta );

	if ( rc != 0 ) {
		DBGC ( xfer, "XFER %p<-%p deliver_iob: %s\n", xfer, dest,
		       strerror ( rc ) );
	}
	xfer_put ( dest );
	return rc;
}

/**
 * Deliver datagram as I/O buffer with metadata
 *
 * @v xfer		Data transfer interface
 * @v iobuf		Datagram I/O buffer
 * @ret rc		Return status code
 */
int xfer_deliver_iob ( struct xfer_interface *xfer,
		       struct io_buffer *iobuf ) {
	return xfer_deliver_iob_meta ( xfer, iobuf, &dummy_metadata );
}

/**
 * Deliver datagram as raw data
 *
 * @v xfer		Data transfer interface
 * @v iobuf		Datagram I/O buffer
 * @ret rc		Return status code
 */
int xfer_deliver_raw ( struct xfer_interface *xfer,
		       const void *data, size_t len ) {
	struct xfer_interface *dest = xfer_get_dest ( xfer );
	int rc;

	DBGC ( xfer, "XFER %p->%p deliver_raw %p+%zd\n", xfer, dest,
	       data, len );

	rc = dest->op->deliver_raw ( dest, data, len );

	if ( rc != 0 ) {
		DBGC ( xfer, "XFER %p<-%p deliver_raw: %s\n", xfer, dest,
		       strerror ( rc ) );
	}
	xfer_put ( dest );
	return rc;
}

/**
 * Deliver formatted string
 *
 * @v xfer		Data transfer interface
 * @v format		Format string
 * @v args		Arguments corresponding to the format string
 * @ret rc		Return status code
 */
int xfer_vprintf ( struct xfer_interface *xfer, const char *format,
		   va_list args ) {
	size_t len;
	va_list args_tmp;

	va_copy ( args_tmp, args );
	len = vsnprintf ( NULL, 0, format, args );
	{
		char buf[len + 1];
		vsnprintf ( buf, sizeof ( buf ), format, args_tmp );
		va_end ( args_tmp );
		return xfer_deliver_raw ( xfer, buf, len );
	}
}

/**
 * Deliver formatted string
 *
 * @v xfer		Data transfer interface
 * @v format		Format string
 * @v ...		Arguments corresponding to the format string
 * @ret rc		Return status code
 */
int xfer_printf ( struct xfer_interface *xfer, const char *format, ... ) {
	va_list args;
	int rc;

	va_start ( args, format );
	rc = xfer_vprintf ( xfer, format, args );
	va_end ( args );
	return rc;
}

/**
 * Seek to position
 *
 * @v xfer		Data transfer interface
 * @v offset		Offset to new position
 * @v whence		Basis for new position
 * @ret rc		Return status code
 */
int xfer_seek ( struct xfer_interface *xfer, off_t offset, int whence ) {
	struct io_buffer *iobuf;
	struct xfer_metadata meta = {
		.offset = offset,
		.whence = whence,
	};

	DBGC ( xfer, "XFER %p seek %s+%ld\n", xfer,
	       whence_text ( whence ), offset );

	/* Allocate and send a zero-length data buffer */
	iobuf = xfer_alloc_iob ( xfer, 0 );
	if ( ! iobuf )
		return -ENOMEM;
	return xfer_deliver_iob_meta ( xfer, iobuf, &meta );
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
 * Ignore close() event
 *
 * @v xfer		Data transfer interface
 * @v rc		Reason for close
 */
void ignore_xfer_close ( struct xfer_interface *xfer __unused,
			 int rc __unused ) {
	/* Nothing to do */
}

/**
 * Ignore vredirect() event
 *
 * @v xfer		Data transfer interface
 * @v type		New location type
 * @v args		Remaining arguments depend upon location type
 * @ret rc		Return status code
 */
int ignore_xfer_vredirect ( struct xfer_interface *xfer __unused,
			    int type __unused, va_list args __unused ) {
	return 0;
}

/**
 * Unlimited flow control window
 *
 * @v xfer		Data transfer interface
 * @ret len		Length of window
 *
 * This handler indicates that the interface is always ready to accept
 * data.
 */
size_t unlimited_xfer_window ( struct xfer_interface *xfer __unused ) {
	return ~( ( size_t ) 0 );
}

/**
 * No flow control window
 *
 * @v xfer		Data transfer interface
 * @ret len		Length of window
 *
 * This handler indicates that the interface is never ready to accept
 * data.
 */
size_t no_xfer_window ( struct xfer_interface *xfer __unused ) {
	return 0;
}

/**
 * Allocate I/O buffer
 *
 * @v xfer		Data transfer interface
 * @v len		I/O buffer payload length
 * @ret iobuf		I/O buffer
 */
struct io_buffer *
default_xfer_alloc_iob ( struct xfer_interface *xfer __unused, size_t len ) {
	return alloc_iob ( len );
}

/**
 * Deliver datagram as raw data
 *
 * @v xfer		Data transfer interface
 * @v iobuf		Datagram I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 *
 * This function is intended to be used as the deliver() method for
 * data transfer interfaces that prefer to handle raw data.
 */
int xfer_deliver_as_raw ( struct xfer_interface *xfer,
			  struct io_buffer *iobuf,
			  struct xfer_metadata *meta __unused ) {
	int rc;

	rc = xfer->op->deliver_raw ( xfer, iobuf->data, iob_len ( iobuf ) );
	free_iob ( iobuf );
	return rc;
}

/**
 * Deliver datagram as I/O buffer
 *
 * @v xfer		Data transfer interface
 * @v data		Data buffer
 * @v len		Length of data buffer
 * @ret rc		Return status code
 *
 * This function is intended to be used as the deliver_raw() method
 * for data transfer interfaces that prefer to handle I/O buffers.
 */
int xfer_deliver_as_iob ( struct xfer_interface *xfer,
			  const void *data, size_t len ) {
	struct io_buffer *iobuf;

	iobuf = xfer->op->alloc_iob ( xfer, len );
	if ( ! iobuf )
		return -ENOMEM;

	memcpy ( iob_put ( iobuf, len ), data, len );
	return xfer->op->deliver_iob ( xfer, iobuf, &dummy_metadata );
}

/**
 * Ignore datagram as raw data event
 *
 * @v xfer		Data transfer interface
 * @v data		Data buffer
 * @v len		Length of data buffer
 * @ret rc		Return status code
 */
int ignore_xfer_deliver_raw ( struct xfer_interface *xfer,
			      const void *data __unused, size_t len ) {
	DBGC ( xfer, "XFER %p %zd bytes delivered %s\n", xfer, len,
	       ( ( xfer == &null_xfer ) ?
		 "before connection" : "after termination" ) );
	return 0;
}

/** Null data transfer interface operations */
struct xfer_interface_operations null_xfer_ops = {
	.close		= ignore_xfer_close,
	.vredirect	= ignore_xfer_vredirect,
	.window		= unlimited_xfer_window,
	.alloc_iob	= default_xfer_alloc_iob,
	.deliver_iob	= xfer_deliver_as_raw,
	.deliver_raw	= ignore_xfer_deliver_raw,
};

/**
 * Null data transfer interface
 *
 * This is the interface to which data transfer interfaces are
 * connected when unplugged.  It will never generate messages, and
 * will silently absorb all received messages.
 */
struct xfer_interface null_xfer = XFER_INIT ( &null_xfer_ops );
