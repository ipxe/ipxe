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

#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <gpxe/xfer.h>
#include <gpxe/open.h>
#include <gpxe/job.h>
#include <gpxe/uaccess.h>
#include <gpxe/umalloc.h>
#include <gpxe/image.h>
#include <gpxe/downloader.h>

/** @file
 *
 * Image downloader
 *
 */

/** A downloader */
struct downloader {
	/** Reference count for this object */
	struct refcnt refcnt;

	/** Job control interface */
	struct job_interface job;
	/** Data transfer interface */
	struct xfer_interface xfer;

	/** Image to contain downloaded file */
	struct image *image;
	/** Current position within image buffer */
	size_t pos;
	/** Image registration routine */
	int ( * register_image ) ( struct image *image );
};

/**
 * Free downloader object
 *
 * @v refcnt		Downloader reference counter
 */
static void downloader_free ( struct refcnt *refcnt ) {
	struct downloader *downloader =
		container_of ( refcnt, struct downloader, refcnt );

	image_put ( downloader->image );
	free ( downloader );
}

/**
 * Terminate download
 *
 * @v downloader	Downloader
 * @v rc		Reason for termination
 */
static void downloader_finished ( struct downloader *downloader, int rc ) {

	/* Block further incoming messages */
	job_nullify ( &downloader->job );
	xfer_nullify ( &downloader->xfer );

	/* Free resources and close interfaces */
	xfer_close ( &downloader->xfer, rc );
	job_done ( &downloader->job, rc );
}

/**
 * Ensure that download buffer is large enough for the specified size
 *
 * @v downloader	Downloader
 * @v len		Required minimum size
 * @ret rc		Return status code
 */
static int downloader_ensure_size ( struct downloader *downloader,
				    size_t len ) {
	userptr_t new_buffer;

	/* If buffer is already large enough, do nothing */
	if ( len <= downloader->image->len )
		return 0;

	DBGC ( downloader, "Downloader %p extending to %zd bytes\n",
	       downloader, len );

	/* Extend buffer */
	new_buffer = urealloc ( downloader->image->data, len );
	if ( ! new_buffer ) {
		DBGC ( downloader, "Downloader %p could not extend buffer to "
		       "%zd bytes\n", downloader, len );
		return -ENOBUFS;
	}
	downloader->image->data = new_buffer;
	downloader->image->len = len;

	return 0;
}

/****************************************************************************
 *
 * Job control interface
 *
 */

/**
 * Handle kill() event received via job control interface
 *
 * @v job		Downloader job control interface
 */
static void downloader_job_kill ( struct job_interface *job ) {
	struct downloader *downloader =
		container_of ( job, struct downloader, job );

	/* Terminate download */
	downloader_finished ( downloader, -ECANCELED );
}

/**
 * Report progress of download job
 *
 * @v job		Downloader job control interface
 * @v progress		Progress report to fill in
 */
static void downloader_job_progress ( struct job_interface *job,
				      struct job_progress *progress ) {
	struct downloader *downloader =
		container_of ( job, struct downloader, job );

	/* This is not entirely accurate, since downloaded data may
	 * arrive out of order (e.g. with multicast protocols), but
	 * it's a reasonable first approximation.
	 */
	progress->completed = downloader->pos;
	progress->total = downloader->image->len;
}

/** Downloader job control interface operations */
static struct job_interface_operations downloader_job_operations = {
	.done		= ignore_job_done,
	.kill		= downloader_job_kill,
	.progress	= downloader_job_progress,
};

/****************************************************************************
 *
 * Data transfer interface
 *
 */

/**
 * Handle deliver_raw() event received via data transfer interface
 *
 * @v xfer		Downloader data transfer interface
 * @v iobuf		Datagram I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 */
static int downloader_xfer_deliver_iob ( struct xfer_interface *xfer,
					 struct io_buffer *iobuf,
					 struct xfer_metadata *meta ) {
	struct downloader *downloader =
		container_of ( xfer, struct downloader, xfer );
	size_t len;
	size_t max;
	int rc;

	/* Calculate new buffer position */
	if ( meta->whence != SEEK_CUR )
		downloader->pos = 0;
	downloader->pos += meta->offset;

	/* Ensure that we have enough buffer space for this data */
	len = iob_len ( iobuf );
	max = ( downloader->pos + len );
	if ( ( rc = downloader_ensure_size ( downloader, max ) ) != 0 )
		goto done;

	/* Copy data to buffer */
	copy_to_user ( downloader->image->data, downloader->pos,
		       iobuf->data, len );

	/* Update current buffer position */
	downloader->pos += len;

 done:
	free_iob ( iobuf );
	return rc;
}

/**
 * Handle close() event received via data transfer interface
 *
 * @v xfer		Downloader data transfer interface
 * @v rc		Reason for close
 */
static void downloader_xfer_close ( struct xfer_interface *xfer, int rc ) {
	struct downloader *downloader =
		container_of ( xfer, struct downloader, xfer );

	/* Register image if download was successful */
	if ( rc == 0 )
		rc = downloader->register_image ( downloader->image );

	/* Terminate download */
	downloader_finished ( downloader, rc );
}

/** Downloader data transfer interface operations */
static struct xfer_interface_operations downloader_xfer_operations = {
	.close		= downloader_xfer_close,
	.vredirect	= xfer_vreopen,
	.window		= unlimited_xfer_window,
	.alloc_iob	= default_xfer_alloc_iob,
	.deliver_iob	= downloader_xfer_deliver_iob,
	.deliver_raw	= xfer_deliver_as_iob,
};

/****************************************************************************
 *
 * Instantiator
 *
 */

/**
 * Instantiate a downloader
 *
 * @v job		Job control interface
 * @v image		Image to fill with downloaded file
 * @v register_image	Image registration routine
 * @v type		Location type to pass to xfer_open()
 * @v ...		Remaining arguments to pass to xfer_open()
 * @ret rc		Return status code
 *
 * Instantiates a downloader object to download the specified URI into
 * the specified image object.  If the download is successful, the
 * image registration routine @c register_image() will be called.
 */
int create_downloader ( struct job_interface *job, struct image *image,
			int ( * register_image ) ( struct image *image ),
			int type, ... ) {
	struct downloader *downloader;
	va_list args;
	int rc;

	/* Allocate and initialise structure */
	downloader = zalloc ( sizeof ( *downloader ) );
	if ( ! downloader )
		return -ENOMEM;
	downloader->refcnt.free = downloader_free;
	job_init ( &downloader->job, &downloader_job_operations,
		   &downloader->refcnt );
	xfer_init ( &downloader->xfer, &downloader_xfer_operations,
		    &downloader->refcnt );
	downloader->image = image_get ( image );
	downloader->register_image = register_image;
	va_start ( args, type );

	/* Instantiate child objects and attach to our interfaces */
	if ( ( rc = xfer_vopen ( &downloader->xfer, type, args ) ) != 0 )
		goto err;

	/* Attach parent interface, mortalise self, and return */
	job_plug_plug ( &downloader->job, job );
	ref_put ( &downloader->refcnt );
	va_end ( args );
	return 0;

 err:
	downloader_finished ( downloader, rc );
	ref_put ( &downloader->refcnt );
	va_end ( args );
	return rc;
}
