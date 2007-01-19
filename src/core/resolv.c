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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <gpxe/in.h>
#include <gpxe/resolv.h>

/** @file
 *
 * Name resolution
 *
 */

static struct async_operations resolv_async_operations;

/** Registered name resolvers */
static struct resolver resolvers[0]
	__table_start ( struct resolver, resolvers );
static struct resolver resolvers_end[0]
	__table_end ( struct resolver, resolvers );

/**
 * Start name resolution
 *
 * @v name		Host name to resolve
 * @v sa		Socket address to fill in
 * @v parent		Parent asynchronous operation
 * @ret rc		Return status code
 */
int resolv ( const char *name, struct sockaddr *sa, struct async *parent ) {
	struct resolution *resolution;
	struct resolver *resolver;
	struct sockaddr_in *sin = ( struct sockaddr_in * ) sa;
	struct in_addr in;
	int rc = -ENXIO;

	/* Allocate and populate resolution structure */
	resolution = malloc ( sizeof ( *resolution ) );
	if ( ! resolution )
		return -ENOMEM;
	async_init ( &resolution->async, &resolv_async_operations, parent );

	/* Check for a dotted quad IP address first */
	if ( inet_aton ( name, &in ) != 0 ) {
		DBGC ( resolution, "RESOLV %p saw valid IP address %s\n",
		       resolution, name );
		sin->sin_family = AF_INET;
		sin->sin_addr = in;
		async_done ( &resolution->async, 0 );
		return 0;
	}

	/* Start up all resolvers */
	for ( resolver = resolvers ; resolver < resolvers_end ; resolver++ ) {
		if ( ( rc = resolver->resolv ( name, sa,
					       &resolution->async ) ) != 0 ) {
			DBGC ( resolution, "RESOLV %p could not start %s: "
			       "%s\n", resolution, resolver->name,
			       strerror ( rc ) );
			/* Continue to try other resolvers */
			continue;
		}
		(resolution->pending)++;
	}
	if ( ! resolution->pending )
		goto err;

	return 0;

 err:
	async_uninit ( &resolution->async );
	free ( resolution );
	return rc;
}

/**
 * Handle child name resolution completion
 *
 * @v async		Name resolution asynchronous operation
 * @v signal		SIGCHLD
 */
static void resolv_sigchld ( struct async *async,
			     enum signal signal __unused ) {
	struct resolution *resolution =
		container_of ( async, struct resolution, async );
	int rc;

	/* If this child succeeded, kill all the others and return */
	async_wait ( async, &rc, 1 );
	if ( rc == 0 ) {
		async_signal_children ( async, SIGKILL );
		async_done ( async, 0 );
		return;
	}

	/* If we have no children left, return failure */
	if ( --(resolution->pending) == 0 )
		async_done ( async, rc );
}

/**
 * Free name resolution structure
 *
 * @v async		Asynchronous operation
 */
static void resolv_reap ( struct async *async ) {
	free ( container_of ( async, struct resolution, async ) );
}

/** Name resolution asynchronous operations */
static struct async_operations resolv_async_operations = {
	.reap = resolv_reap,
	.signal = {
		[SIGCHLD] = resolv_sigchld,
	},
};
