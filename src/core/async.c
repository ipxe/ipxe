/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <gpxe/process.h>
#include <gpxe/async.h>

/** @file
 *
 * Asynchronous operations
 *
 */

/**
 * Wait for asynchronous operation to complete
 *
 * @v aop		Asynchronous operation
 * @ret rc		Return status code
 *
 * Blocks until the specified asynchronous operation has completed.
 * The return status is the return status of the asynchronous
 * operation itself.
 */
int async_wait ( struct async_operation *aop ) {
	int rc;

	/* Wait for operation to complete */
	do {
		step();
		rc = async_status ( aop );
	} while ( rc == -EINPROGRESS );
	
	/* Prepare for next call to async_wait() */
	async_set_status ( aop, -EINPROGRESS );

	return rc;
}
