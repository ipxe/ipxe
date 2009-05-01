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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <gpxe/in.h>
#include <gpxe/xfer.h>
#include <gpxe/open.h>
#include <gpxe/process.h>
#include <gpxe/resolv.h>

/** @file
 *
 * Name resolution
 *
 */

/***************************************************************************
 *
 * Name resolution interfaces
 *
 ***************************************************************************
 */

/**
 * Name resolution completed
 *
 * @v resolv		Name resolution interface
 * @v sa		Completed socket address (if successful)
 * @v rc		Final status code
 */
void resolv_done ( struct resolv_interface *resolv, struct sockaddr *sa,
		   int rc ) {
	struct resolv_interface *dest = resolv_get_dest ( resolv );

	resolv_unplug ( resolv );
	dest->op->done ( dest, sa, rc );
	resolv_put ( dest );
}

/**
 * Ignore name resolution done() event
 *
 * @v resolv		Name resolution interface
 * @v sa		Completed socket address (if successful)
 * @v rc		Final status code
 */
void ignore_resolv_done ( struct resolv_interface *resolv __unused,
			  struct sockaddr *sa __unused, int rc __unused ) {
	/* Do nothing */
}

/** Null name resolution interface operations */
struct resolv_interface_operations null_resolv_ops = {
	.done		= ignore_resolv_done,
};

/** Null name resolution interface */
struct resolv_interface null_resolv = {
	.intf = {
		.dest = &null_resolv.intf,
		.refcnt = NULL,
	},
	.op = &null_resolv_ops,
};

/***************************************************************************
 *
 * Numeric name resolver
 *
 ***************************************************************************
 */

/** A numeric name resolver */
struct numeric_resolv {
	/** Reference counter */
	struct refcnt refcnt;
	/** Name resolution interface */
	struct resolv_interface resolv;
	/** Process */
	struct process process;
	/** Completed socket address */
	struct sockaddr sa;
	/** Overall status code */
	int rc;
};

static void numeric_step ( struct process *process ) {
	struct numeric_resolv *numeric =
		container_of ( process, struct numeric_resolv, process );
	
	resolv_done ( &numeric->resolv, &numeric->sa, numeric->rc );
	process_del ( process );
}

static int numeric_resolv ( struct resolv_interface *resolv,
			    const char *name, struct sockaddr *sa ) {
	struct numeric_resolv *numeric;
	struct sockaddr_in *sin;

	/* Allocate and initialise structure */
	numeric = zalloc ( sizeof ( *numeric ) );
	if ( ! numeric )
		return -ENOMEM;
	resolv_init ( &numeric->resolv, &null_resolv_ops, &numeric->refcnt );
	process_init ( &numeric->process, numeric_step, &numeric->refcnt );
	memcpy ( &numeric->sa, sa, sizeof ( numeric->sa ) );

	DBGC ( numeric, "NUMERIC %p attempting to resolve \"%s\"\n",
	       numeric, name );

	/* Attempt to resolve name */
	sin = ( ( struct sockaddr_in * ) &numeric->sa );
	sin->sin_family = AF_INET;
	if ( inet_aton ( name, &sin->sin_addr ) == 0 )
		numeric->rc = -EINVAL;

	/* Attach to parent interface, mortalise self, and return */
	resolv_plug_plug ( &numeric->resolv, resolv );
	ref_put ( &numeric->refcnt );
	return 0;
}

struct resolver numeric_resolver __resolver ( RESOLV_NUMERIC ) = {
	.name = "NUMERIC",
	.resolv = numeric_resolv,
};

/***************************************************************************
 *
 * Name resolution multiplexer
 *
 ***************************************************************************
 */

/** A name resolution multiplexer */
struct resolv_mux {
	/** Reference counter */
	struct refcnt refcnt;
	/** Parent name resolution interface */
	struct resolv_interface parent;

	/** Child name resolution interface */
	struct resolv_interface child;
	/** Current child resolver */
	struct resolver *resolver;

	/** Socket address to complete */
	struct sockaddr sa;
	/** Name to be resolved
	 *
	 * Must be at end of structure
	 */
	char name[0];
};

/**
 * Try current child name resolver
 *
 * @v mux		Name resolution multiplexer
 * @ret rc		Return status code
 */
static int resolv_mux_try ( struct resolv_mux *mux ) {
	struct resolver *resolver = mux->resolver;
	int rc;

	DBGC ( mux, "RESOLV %p trying method %s\n", mux, resolver->name );

	if ( ( rc = resolver->resolv ( &mux->child, mux->name,
				       &mux->sa ) ) != 0 ) {
		DBGC ( mux, "RESOLV %p could not use method %s: %s\n",
		       mux, resolver->name, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Handle done() event from child name resolver
 *
 * @v resolv		Child name resolution interface
 * @v sa		Completed socket address (if successful)
 * @v rc		Final status code
 */
static void resolv_mux_done ( struct resolv_interface *resolv,
			      struct sockaddr *sa, int rc ) {
	struct resolv_mux *mux =
		container_of ( resolv, struct resolv_mux, child );

	/* Unplug child */
	resolv_unplug ( &mux->child );

	/* If this resolution succeeded, stop now */
	if ( rc == 0 ) {
		DBGC ( mux, "RESOLV %p succeeded using method %s\n",
		       mux, mux->resolver->name );
		goto finished;
	}

	/* Attempt next child resolver, if possible */
	mux->resolver++;
	if ( mux->resolver >= table_end ( RESOLVERS ) ) {
		DBGC ( mux, "RESOLV %p failed to resolve name\n", mux );
		goto finished;
	}
	if ( ( rc = resolv_mux_try ( mux ) ) != 0 )
		goto finished;

	/* Next resolver is now running */
	return;
	
 finished:
	resolv_done ( &mux->parent, sa, rc );
}

/** Name resolution multiplexer operations */
static struct resolv_interface_operations resolv_mux_child_ops = {
	.done		= resolv_mux_done,
};

/**
 * Start name resolution
 *
 * @v resolv		Name resolution interface
 * @v name		Name to resolve
 * @v sa		Socket address to complete
 * @ret rc		Return status code
 */
int resolv ( struct resolv_interface *resolv, const char *name,
	     struct sockaddr *sa ) {
	struct resolv_mux *mux;
	size_t name_len = ( strlen ( name ) + 1 );
	int rc;

	/* Allocate and initialise structure */
	mux = zalloc ( sizeof ( *mux ) + name_len );
	if ( ! mux )
		return -ENOMEM;
	resolv_init ( &mux->parent, &null_resolv_ops, &mux->refcnt );
	resolv_init ( &mux->child, &resolv_mux_child_ops, &mux->refcnt );
	mux->resolver = table_start ( RESOLVERS );
	memcpy ( &mux->sa, sa, sizeof ( mux->sa ) );
	memcpy ( mux->name, name, name_len );

	DBGC ( mux, "RESOLV %p attempting to resolve \"%s\"\n", mux, name );

	/* Start first resolver in chain.  There will always be at
	 * least one resolver (the numeric resolver), so no need to
	 * check for the zero-resolvers-available case.
	 */
	if ( ( rc = resolv_mux_try ( mux ) ) != 0 )
		goto err;

	/* Attach parent interface, mortalise self, and return */
	resolv_plug_plug ( &mux->parent, resolv );
	ref_put ( &mux->refcnt );
	return 0;

 err:
	ref_put ( &mux->refcnt );
	return rc;	
}

/***************************************************************************
 *
 * Named socket opening
 *
 ***************************************************************************
 */

/** A named socket */
struct named_socket {
	/** Reference counter */
	struct refcnt refcnt;
	/** Data transfer interface */
	struct xfer_interface xfer;
	/** Name resolution interface */
	struct resolv_interface resolv;
	/** Communication semantics (e.g. SOCK_STREAM) */
	int semantics;
	/** Stored local socket address, if applicable */
	struct sockaddr local;
	/** Stored local socket address exists */
	int have_local;
};

/**
 * Finish using named socket
 *
 * @v named		Named socket
 * @v rc		Reason for finish
 */
static void named_done ( struct named_socket *named, int rc ) {

	/* Close all interfaces */
	resolv_nullify ( &named->resolv );
	xfer_nullify ( &named->xfer );
	xfer_close ( &named->xfer, rc );
}

/**
 * Handle close() event
 *
 * @v xfer		Data transfer interface
 * @v rc		Reason for close
 */
static void named_xfer_close ( struct xfer_interface *xfer, int rc ) {
	struct named_socket *named =
		container_of ( xfer, struct named_socket, xfer );

	named_done ( named, rc );
}

/** Named socket opener data transfer interface operations */
static struct xfer_interface_operations named_xfer_ops = {
	.close		= named_xfer_close,
	.vredirect	= ignore_xfer_vredirect,
	.window		= no_xfer_window,
	.alloc_iob	= default_xfer_alloc_iob,
	.deliver_iob	= xfer_deliver_as_raw,
	.deliver_raw	= ignore_xfer_deliver_raw,
};

/**
 * Handle done() event
 *
 * @v resolv		Name resolution interface
 * @v sa		Completed socket address (if successful)
 * @v rc		Final status code
 */
static void named_resolv_done ( struct resolv_interface *resolv,
				struct sockaddr *sa, int rc ) {
	struct named_socket *named =
		container_of ( resolv, struct named_socket, resolv );

	/* Redirect if name resolution was successful */
	if ( rc == 0 ) {
		rc = xfer_redirect ( &named->xfer, LOCATION_SOCKET,
				     named->semantics, sa,
				     ( named->have_local ?
				       &named->local : NULL ) );
	}

	/* Terminate resolution */
	named_done ( named, rc );
}

/** Named socket opener name resolution interface operations */
static struct resolv_interface_operations named_resolv_ops = {
	.done		= named_resolv_done,
};

/**
 * Open named socket
 *
 * @v semantics		Communication semantics (e.g. SOCK_STREAM)
 * @v peer		Peer socket address to complete
 * @v name		Name to resolve
 * @v local		Local socket address, or NULL
 * @ret rc		Return status code
 */
int xfer_open_named_socket ( struct xfer_interface *xfer, int semantics,
			     struct sockaddr *peer, const char *name,
			     struct sockaddr *local ) {
	struct named_socket *named;
	int rc;

	/* Allocate and initialise structure */
	named = zalloc ( sizeof ( *named ) );
	if ( ! named )
		return -ENOMEM;
	xfer_init ( &named->xfer, &named_xfer_ops, &named->refcnt );
	resolv_init ( &named->resolv, &named_resolv_ops, &named->refcnt );
	named->semantics = semantics;
	if ( local ) {
		memcpy ( &named->local, local, sizeof ( named->local ) );
		named->have_local = 1;
	}

	DBGC ( named, "RESOLV %p opening named socket \"%s\"\n",
	       named, name );

	/* Start name resolution */
	if ( ( rc = resolv ( &named->resolv, name, peer ) ) != 0 )
		goto err;

	/* Attach parent interface, mortalise self, and return */
	xfer_plug_plug ( &named->xfer, xfer );
	ref_put ( &named->refcnt );
	return 0;

 err:
	ref_put ( &named->refcnt );
	return rc;
}
