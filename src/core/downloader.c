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

#include <stdlib.h>
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
	image_put ( downloader->image );
	xfer_close ( &downloader->xfer, rc );
	job_done ( &downloader->job, rc );

	/* Drop reference to self */
	ref_put ( &downloader->refcnt );
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
 * Handle start() event received via job control interface
 *
 * @v job		Downloader job control interface
 */
static void downloader_job_start ( struct job_interface *job ) {
	struct downloader *downloader = 
		container_of ( job, struct downloader, job );

	/* Start data transfer */
	xfer_start ( &downloader->xfer );
}

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

/** Downloader job control interface operations */
static struct job_interface_operations downloader_job_operations = {
	.start		= downloader_job_start,
	.done		= ignore_job_done,
	.kill		= downloader_job_kill,
	.progress	= ignore_job_progress,
};

/****************************************************************************
 *
 * Data transfer interface
 *
 */

/**
 * Handle seek() event received via data transfer interface
 *
 * @v xfer		Downloader data transfer interface
 * @v pos		New position
 * @ret rc		Return status code
 */
static int downloader_xfer_seek ( struct xfer_interface *xfer, size_t pos ) {
	struct downloader *downloader =
		container_of ( xfer, struct downloader, xfer );
	int rc;

	/* Ensure that we have enough buffer space for this buffer position */
	if ( ( rc = downloader_ensure_size ( downloader, pos ) ) != 0 )
		return rc;

	/* Update current buffer position */
	downloader->pos = pos;

	return 0;
}

/**
 * Handle deliver_raw() event received via data transfer interface
 *
 * @v xfer		Downloader data transfer interface
 * @v data		Data buffer
 * @v len		Length of data buffer
 * @ret rc		Return status code
 */
static int downloader_xfer_deliver_raw ( struct xfer_interface *xfer,
					 const void *data, size_t len ) {
	struct downloader *downloader =
		container_of ( xfer, struct downloader, xfer );
	size_t max;
	int rc;

	/* Ensure that we have enough buffer space for this data */
	max = ( downloader->pos + len );
	if ( ( rc = downloader_ensure_size ( downloader, max ) ) != 0 )
		return rc;

	/* Copy data to buffer */
	copy_to_user ( downloader->image->data, downloader->pos, data, len );

	/* Update current buffer position */
	downloader->pos += len;

	return 0;
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
	.start		= ignore_xfer_start,
	.close		= downloader_xfer_close,
	.vredirect	= default_xfer_vredirect,
	.seek		= downloader_xfer_seek,
	.deliver	= xfer_deliver_as_raw,
	.deliver_raw	= downloader_xfer_deliver_raw,
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
 * @v uri_string	URI string
 * @v image		Image to fill with downloaded file
 * @v register_image	Image registration routine
 * @ret rc		Return status code
 *
 * Instantiates a downloader object to download the specified URI into
 * the specified image object.  If the download is successful, the
 * image registration routine @c register_image() will be called.
 */
int create_downloader ( struct job_interface *job, const char *uri_string,
			struct image *image,
			int ( * register_image ) ( struct image *image ) ) {
	struct downloader *downloader;
	int rc;

	/* Allocate and initialise structure */
	downloader = malloc ( sizeof ( *downloader ) );
	if ( ! downloader )
		return -ENOMEM;
	memset ( downloader, 0, sizeof ( *downloader ) );
	job_init ( &downloader->job, &downloader_job_operations,
		   &downloader->refcnt );
	xfer_init ( &downloader->xfer, &downloader_xfer_operations,
		    &downloader->refcnt );
	downloader->image = image_get ( image );
	downloader->register_image = register_image;

	/* Instantiate child objects and attach to our interfaces */
	if ( ( rc = open ( &downloader->xfer, LOCATION_URI,
			   uri_string ) ) != 0 )
		goto err;

	/* Attach parent interface and return */
	job_plug_plug ( &downloader->job, job );
	return 0;

 err:
	downloader_finished ( downloader, rc );
	return rc;
}
