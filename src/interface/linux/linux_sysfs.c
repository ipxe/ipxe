/*
 * Copyright (C) 2021 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <string.h>
#include <errno.h>
#include <ipxe/umalloc.h>
#include <ipxe/linux_api.h>
#include <ipxe/linux.h>
#include <ipxe/linux_sysfs.h>

/** @file
 *
 * Linux sysfs files
 *
 */

/** Read blocksize */
#define LINUX_SYSFS_BLKSIZE 4096

/**
 * Read file from sysfs
 *
 * @v filename		Filename
 * @v data		Data to fill in
 * @ret len		Length read, or negative error
 */
int linux_sysfs_read ( const char *filename, void **data ) {
	void *tmp;
	ssize_t read;
	size_t len;
	int fd;
	int rc;

	/* Open file */
	fd = linux_open ( filename, O_RDONLY );
	if ( fd < 0 ) {
		rc = -ELINUX ( linux_errno );
		DBGC ( filename, "LINUX could not open %s: %s\n",
		       filename, linux_strerror ( linux_errno ) );
		goto err_open;
	}

	/* Read file */
	for ( *data = NULL, len = 0 ; ; len += read ) {

		/* (Re)allocate space */
		tmp = urealloc ( *data, ( len + LINUX_SYSFS_BLKSIZE ) );
		if ( ! tmp ) {
			rc = -ENOMEM;
			goto err_alloc;
		}
		*data = tmp;

		/* Read from file */
		read = linux_read ( fd, ( *data + len ), LINUX_SYSFS_BLKSIZE );
		if ( read == 0 )
			break;
		if ( read < 0 ) {
			DBGC ( filename, "LINUX could not read %s: %s\n",
			       filename, linux_strerror ( linux_errno ) );
			goto err_read;
		}
	}

	/* Close file */
	linux_close ( fd );

	DBGC ( filename, "LINUX read %s\n", filename );
	return len;

 err_read:
 err_alloc:
	ufree ( *data );
	linux_close ( fd );
 err_open:
	return rc;
}
