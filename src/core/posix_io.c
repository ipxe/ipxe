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
#include <string.h>
#include <errno.h>
#include <gpxe/list.h>
#include <gpxe/xfer.h>
#include <gpxe/open.h>
#include <gpxe/process.h>
#include <gpxe/posix_io.h>

/** @file
 *
 * POSIX-like I/O
 *
 * These functions provide traditional blocking I/O semantics.  They
 * are designed to be used by the PXE TFTP API.  Because they block,
 * they may not be used by most other portions of the gPXE codebase.
 */

/** An open file */
struct posix_file {
	/** Reference count for this object */
	struct refcnt refcnt;
	/** List of open files */
	struct list_head list;
	/** File descriptor */
	int fd;
	/** Overall status
	 *
	 * Set to -EINPROGRESS while data transfer is in progress.
	 */
	int rc;
	/** Data transfer interface */
	struct xfer_interface xfer;
	/** Current seek position */
	size_t pos;
	/** File size */
	size_t filesize;
	/** Received data queue */
	struct list_head data;
};

/** List of open files */
static LIST_HEAD ( posix_files );

/**
 * Free open file
 *
 * @v refcnt		Reference counter
 */
static void posix_file_free ( struct refcnt *refcnt ) {
	struct posix_file *file =
		container_of ( refcnt, struct posix_file, refcnt );
	struct io_buffer *iobuf;
	struct io_buffer *tmp;

	list_for_each_entry_safe ( iobuf, tmp, &file->data, list ) {
		list_del ( &iobuf->list );
		free_iob ( iobuf );
	}
	free ( file );
}

/**
 * Terminate file data transfer
 *
 * @v file		POSIX file
 * @v rc		Reason for termination
 */
static void posix_file_finished ( struct posix_file *file, int rc ) {
	xfer_nullify ( &file->xfer );
	xfer_close ( &file->xfer, rc );
	file->rc = rc;
}

/**
 * Handle close() event
 *
 * @v xfer		POSIX file data transfer interface
 * @v rc		Reason for close
 */
static void posix_file_xfer_close ( struct xfer_interface *xfer, int rc ) {
	struct posix_file *file =
		container_of ( xfer, struct posix_file, xfer );

	posix_file_finished ( file, rc );
}

/**
 * Handle deliver_iob() event
 *
 * @v xfer		POSIX file data transfer interface
 * @v iobuf		I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 */
static int
posix_file_xfer_deliver_iob ( struct xfer_interface *xfer,
			      struct io_buffer *iobuf,
			      struct xfer_metadata *meta ) {
	struct posix_file *file =
		container_of ( xfer, struct posix_file, xfer );

	/* Keep track of file position solely for the filesize */
	if ( meta->whence != SEEK_CUR )
		file->pos = 0;
	file->pos += meta->offset;
	if ( file->filesize < file->pos )
		file->filesize = file->pos;

	if ( iob_len ( iobuf ) ) {
		list_add_tail ( &iobuf->list, &file->data );
	} else {
		free_iob ( iobuf );
	}

	return 0;
}

/** POSIX file data transfer interface operations */
static struct xfer_interface_operations posix_file_xfer_operations = {
	.close		= posix_file_xfer_close,
	.vredirect	= xfer_vreopen,
	.window		= unlimited_xfer_window,
	.alloc_iob	= default_xfer_alloc_iob,
	.deliver_iob	= posix_file_xfer_deliver_iob,
	.deliver_raw	= xfer_deliver_as_iob,
};

/**
 * Identify file by file descriptor
 *
 * @v fd		File descriptor
 * @ret file		Corresponding file, or NULL
 */
static struct posix_file * posix_fd_to_file ( int fd ) {
	struct posix_file *file;

	list_for_each_entry ( file, &posix_files, list ) {
		if ( file->fd == fd )
			return file;
	}
	return NULL;
}

/**
 * Find an available file descriptor
 *
 * @ret fd		File descriptor, or negative error number
 */
static int posix_find_free_fd ( void ) {
	int fd;

	for ( fd = POSIX_FD_MIN ; fd <= POSIX_FD_MAX ; fd++ ) {
		if ( ! posix_fd_to_file ( fd ) )
			return fd;
	}
	DBG ( "POSIX could not find free file descriptor\n" );
	return -ENFILE;
}

/**
 * Open file
 *
 * @v uri_string	URI string
 * @ret fd		File descriptor, or negative error number
 */
int open ( const char *uri_string ) {
	struct posix_file *file;
	int fd;
	int rc;

	/* Find a free file descriptor to use */
	fd = posix_find_free_fd();
	if ( fd < 0 )
		return fd;

	/* Allocate and initialise structure */
	file = zalloc ( sizeof ( *file ) );
	if ( ! file )
		return -ENOMEM;
	file->refcnt.free = posix_file_free;
	file->fd = fd;
	file->rc = -EINPROGRESS;
	xfer_init ( &file->xfer, &posix_file_xfer_operations,
		    &file->refcnt );
	INIT_LIST_HEAD ( &file->data );

	/* Open URI on data transfer interface */
	if ( ( rc = xfer_open_uri_string ( &file->xfer, uri_string ) ) != 0 )
		goto err;

	/* Wait for open to succeed or fail */
	while ( list_empty ( &file->data ) ) {
		step();
		if ( file->rc == 0 )
			break;
		if ( file->rc != -EINPROGRESS ) {
			rc = file->rc;
			goto err;
		}
	}

	/* Add to list of open files.  List takes reference ownership. */
	list_add ( &file->list, &posix_files );
	DBG ( "POSIX opened %s as file %d\n", uri_string, fd );
	return fd;

 err:
	posix_file_finished ( file, rc );
	ref_put ( &file->refcnt );
	return rc;
}

/**
 * Check file descriptors for readiness
 *
 * @v readfds		File descriptors to check
 * @v wait		Wait until data is ready
 * @ret nready		Number of ready file descriptors
 */
int select ( fd_set *readfds, int wait ) {
	struct posix_file *file;
	int fd;

	do {
		for ( fd = POSIX_FD_MIN ; fd <= POSIX_FD_MAX ; fd++ ) {
			if ( ! FD_ISSET ( fd, readfds ) )
				continue;
			file = posix_fd_to_file ( fd );
			if ( ! file )
				return -EBADF;
			if ( ( list_empty ( &file->data ) ) &&
			     ( file->rc == -EINPROGRESS ) )
				continue;
			/* Data is available or status has changed */
			FD_ZERO ( readfds );
			FD_SET ( fd, readfds );
			return 1;
		}
		step();
	} while ( wait );

	return 0;
}

/**
 * Read data from file
 *
 * @v buffer		Data buffer
 * @v offset		Starting offset within data buffer
 * @v len		Maximum length to read
 * @ret len		Actual length read, or negative error number
 *
 * This call is non-blocking; if no data is available to read then
 * -EWOULDBLOCK will be returned.
 */
ssize_t read_user ( int fd, userptr_t buffer, off_t offset, size_t max_len ) {
	struct posix_file *file;
	struct io_buffer *iobuf;
	size_t len;

	/* Identify file */
	file = posix_fd_to_file ( fd );
	if ( ! file )
		return -EBADF;

	/* Try to fetch more data if none available */
	if ( list_empty ( &file->data ) )
		step();

	/* Dequeue at most one received I/O buffer into user buffer */
	list_for_each_entry ( iobuf, &file->data, list ) {
		len = iob_len ( iobuf );
		if ( len > max_len )
			len = max_len;
		copy_to_user ( buffer, offset, iobuf->data, len );
		iob_pull ( iobuf, len );
		if ( ! iob_len ( iobuf ) ) {
			list_del ( &iobuf->list );
			free_iob ( iobuf );
		}
		file->pos += len;
		assert ( len != 0 );
		return len;
	}

	/* If file has completed, return (after returning all data) */
	if ( file->rc != -EINPROGRESS ) {
		assert ( list_empty ( &file->data ) );
		return file->rc;
	}

	/* No data ready and file still in progress; return -WOULDBLOCK */
	return -EWOULDBLOCK;
}

/**
 * Determine file size
 *
 * @v fd		File descriptor
 * @ret size		File size, or negative error number
 */
ssize_t fsize ( int fd ) {
	struct posix_file *file;

	/* Identify file */
	file = posix_fd_to_file ( fd );
	if ( ! file )
		return -EBADF;

	return file->filesize;
}

/**
 * Close file
 *
 * @v fd		File descriptor
 * @ret rc		Return status code
 */
int close ( int fd ) {
	struct posix_file *file;

	/* Identify file */
	file = posix_fd_to_file ( fd );
	if ( ! file )
		return -EBADF;

	/* Terminate data transfer */
	posix_file_finished ( file, 0 );

	/* Remove from list of open files and drop reference */
	list_del ( &file->list );
	ref_put ( &file->refcnt );
	return 0;
}
