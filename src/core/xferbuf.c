/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ipxe/xfer.h>
#include <ipxe/iobuf.h>
#include <ipxe/umalloc.h>
#include <ipxe/profile.h>
#include <ipxe/image.h>
#include <ipxe/xferbuf.h>

/** @file
 *
 * Data transfer buffer
 *
 */

/** Data delivery profiler */
static struct profiler xferbuf_deliver_profiler __profiler =
	{ .name = "xferbuf.deliver" };

/** Data write profiler */
static struct profiler xferbuf_write_profiler __profiler =
	{ .name = "xferbuf.write" };

/** Data read profiler */
static struct profiler xferbuf_read_profiler __profiler =
	{ .name = "xferbuf.read" };

/**
 * Free data transfer buffer
 *
 * @v xferbuf		Data transfer buffer
 */
void xferbuf_free ( struct xfer_buffer *xferbuf ) {

	xferbuf->op->realloc ( xferbuf, 0 );
	xferbuf->len = 0;
	xferbuf->max = 0;
	xferbuf->pos = 0;
}

/**
 * Ensure that data transfer buffer is large enough for the specified size
 *
 * @v xferbuf		Data transfer buffer
 * @v len		Required minimum size
 * @ret rc		Return status code
 */
static int xferbuf_ensure_size ( struct xfer_buffer *xferbuf, size_t len ) {
	int rc;

	/* Record maximum required size */
	if ( len > xferbuf->max )
		xferbuf->max = len;

	/* If buffer is already large enough, do nothing */
	if ( len <= xferbuf->len )
		return 0;

	/* Extend buffer */
	if ( ( rc = xferbuf->op->realloc ( xferbuf, len ) ) != 0 ) {
		DBGC ( xferbuf, "XFERBUF %p could not extend buffer to "
		       "%zd bytes: %s\n", xferbuf, len, strerror ( rc ) );
		return rc;
	}
	xferbuf->len = len;

	return 0;
}

/**
 * Write to data transfer buffer
 *
 * @v xferbuf		Data transfer buffer
 * @v offset		Starting offset
 * @v data		Data to write
 * @v len		Length of data
 */
int xferbuf_write ( struct xfer_buffer *xferbuf, size_t offset,
		    const void *data, size_t len ) {
	size_t max_len;
	void *raw;
	int rc;

	/* Check for overflow */
	max_len = ( offset + len );
	if ( max_len < offset )
		return -EOVERFLOW;

	/* Ensure buffer is large enough to contain this write */
	if ( ( rc = xferbuf_ensure_size ( xferbuf, max_len ) ) != 0 )
		return rc;

	/* Copy data to buffer (if non-void) */
	raw = xferbuf->op->access ( xferbuf );
	profile_start ( &xferbuf_write_profiler );
	if ( raw )
		memcpy ( ( raw + offset ), data, len );
	profile_stop ( &xferbuf_write_profiler );

	return 0;
}

/**
 * Read from data transfer buffer
 *
 * @v xferbuf		Data transfer buffer
 * @v offset		Starting offset
 * @v data		Data to write
 * @v len		Length of data
 */
int xferbuf_read ( struct xfer_buffer *xferbuf, size_t offset,
		   void *data, size_t len ) {
	const void *raw;

	/* Check that read is within buffer range */
	if ( ( offset > xferbuf->len ) ||
	     ( len > ( xferbuf->len - offset ) ) )
		return -ENOENT;

	/* Access raw data buffer */
	raw = xferbuf->op->access ( xferbuf );

	/* Check that buffer is non-void */
	if ( len && ( ! raw ) )
		return -ENOTTY;

	/* Copy data from buffer */
	profile_start ( &xferbuf_read_profiler );
	memcpy ( data, ( raw + offset ), len );
	profile_stop ( &xferbuf_read_profiler );

	return 0;
}

/**
 * Add received data to data transfer buffer
 *
 * @v xferbuf		Data transfer buffer
 * @v iobuf		I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 */
int xferbuf_deliver ( struct xfer_buffer *xferbuf, struct io_buffer *iobuf,
		      struct xfer_metadata *meta ) {
	size_t len = iob_len ( iobuf );
	size_t pos;
	int rc;

	/* Start profiling */
	profile_start ( &xferbuf_deliver_profiler );

	/* Calculate new buffer position */
	pos = xferbuf->pos;
	if ( meta->flags & XFER_FL_ABS_OFFSET )
		pos = 0;
	pos += meta->offset;

	/* Write data to buffer */
	if ( ( rc = xferbuf_write ( xferbuf, pos, iobuf->data, len ) ) != 0 )
		goto done;

	/* Update current buffer position */
	xferbuf->pos = ( pos + len );

 done:
	free_iob ( iobuf );
	profile_stop ( &xferbuf_deliver_profiler );
	return rc;
}

/**
 * Access raw pointer based data buffer
 *
 * @v xferbuf		Data transfer buffer
 * @ret raw		Raw data pointer
 */
static void * xferbuf_raw_access ( struct xfer_buffer *xferbuf ) {

	return xferbuf->data;
}

/**
 * Reallocate malloc()-based data transfer buffer
 *
 * @v xferbuf		Data transfer buffer
 * @v len		New length (or zero to free buffer)
 * @ret rc		Return status code
 */
static int xferbuf_malloc_realloc ( struct xfer_buffer *xferbuf, size_t len ) {
	void *new_data;

	new_data = realloc ( xferbuf->data, len );
	if ( ! new_data )
		return -ENOSPC;
	xferbuf->data = new_data;
	return 0;
}

/** malloc()-based data buffer operations */
struct xfer_buffer_operations xferbuf_malloc_operations = {
	.realloc = xferbuf_malloc_realloc,
	.access = xferbuf_raw_access,
};

/**
 * Reallocate umalloc()-based data transfer buffer
 *
 * @v xferbuf		Data transfer buffer
 * @v len		New length (or zero to free buffer)
 * @ret rc		Return status code
 */
static int xferbuf_umalloc_realloc ( struct xfer_buffer *xferbuf, size_t len ) {
	void *new_udata;

	new_udata = urealloc ( xferbuf->data, len );
	if ( ! new_udata )
		return -ENOSPC;
	xferbuf->data = new_udata;
	return 0;
}

/** umalloc()-based data buffer operations */
struct xfer_buffer_operations xferbuf_umalloc_operations = {
	.realloc = xferbuf_umalloc_realloc,
	.access = xferbuf_raw_access,
};

/**
 * Reallocate fixed-size data transfer buffer
 *
 * @v xferbuf		Data transfer buffer
 * @v len		New length (or zero to free buffer)
 * @ret rc		Return status code
 */
static int xferbuf_fixed_realloc ( struct xfer_buffer *xferbuf, size_t len ) {

	/* Refuse to allocate extra space */
	if ( len > xferbuf->len ) {
		/* Note that EFI relies upon this error mapping to
		 * EFI_BUFFER_TOO_SMALL.
		 */
		return -ERANGE;
	}

	return 0;
}

/** Fixed-size data buffer operations */
struct xfer_buffer_operations xferbuf_fixed_operations = {
	.realloc = xferbuf_fixed_realloc,
	.access = xferbuf_raw_access,
};

/**
 * Reallocate void data transfer buffer
 *
 * @v xferbuf		Data transfer buffer
 * @v len		New length (or zero to free buffer)
 * @ret rc		Return status code
 */
static int xferbuf_void_realloc ( struct xfer_buffer *xferbuf,
				  size_t len __unused ) {

	/* Succeed without ever allocating data */
	assert ( xferbuf->data == NULL );
	return 0;
}

/** Void data buffer operations */
struct xfer_buffer_operations xferbuf_void_operations = {
	.realloc = xferbuf_void_realloc,
	.access = xferbuf_raw_access,
};

/**
 * Reallocate image-based data transfer buffer
 *
 * @v xferbuf		Data transfer buffer
 * @v len		New length (or zero to free buffer)
 * @ret rc		Return status code
 */
static int xferbuf_image_realloc ( struct xfer_buffer *xferbuf, size_t len ) {
	struct image *image = xferbuf->data;
	int rc;

	/* Resize image */
	if ( ( rc = image_set_len ( image, len ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Access image-based data transfer buffer
 *
 * @v xferbuf		Data transfer buffer
 * @ret raw		Raw data pointer
 */
static void * xferbuf_image_access ( struct xfer_buffer *xferbuf ) {
	struct image *image = xferbuf->data;

	return image->rwdata;
}

/** Image-based data buffer operations */
struct xfer_buffer_operations xferbuf_image_operations = {
	.realloc = xferbuf_image_realloc,
	.access = xferbuf_image_access,
};

/**
 * Get underlying data transfer buffer
 *
 * @v interface		Data transfer interface
 * @ret xferbuf		Data transfer buffer, or NULL on error
 *
 * This call will check that the xfer_buffer() handler belongs to the
 * destination interface which also provides xfer_deliver() for this
 * interface.
 *
 * This is done to prevent accidental accesses to a data transfer
 * buffer which may be located behind a non-transparent datapath via a
 * series of pass-through interfaces.
 */
struct xfer_buffer * xfer_buffer ( struct interface *intf ) {
	struct interface *dest;
	xfer_buffer_TYPE ( void * ) *op =
		intf_get_dest_op ( intf, xfer_buffer, &dest );
	void *object = intf_object ( dest );
	struct interface *xfer_deliver_dest;
	struct xfer_buffer *xferbuf;

	/* Check that this operation is provided by the same interface
	 * which handles xfer_deliver().
	 */
	( void ) intf_get_dest_op ( intf, xfer_deliver, &xfer_deliver_dest );

	if ( op && ( dest == xfer_deliver_dest ) ) {
		xferbuf = op ( object );
	} else {
		/* Default is to not have a data transfer buffer */
		xferbuf = NULL;
	}

	intf_put ( xfer_deliver_dest );
	intf_put ( dest );
	return xferbuf;
}
