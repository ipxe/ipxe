#ifndef _GPXE_RESOLV_H
#define _GPXE_RESOLV_H

/** @file
 *
 * Name resolution
 *
 */

struct sockaddr;

#include <gpxe/async.h>
#include <gpxe/tables.h>

/** A name resolver */
struct resolver {
	/** Name of this resolver (e.g. "DNS") */
	const char *name;
	/** Start name resolution
	 *
	 * @v name		Host name to resolve
	 * @v sa		Socket address to fill in
	 * @v parent		Parent asynchronous operation
	 * @ret rc		Return status code
	 *
	 * The asynchronous process must be prepared to accept
	 * SIGKILL.
	 */
	int ( * resolv ) ( const char *name, struct sockaddr *sa,
			   struct async *parent );
};

/** A name resolution in progress */
struct resolution {
	/** Asynchronous operation */
	struct async async;
	/** Numner of active child resolvers */
	unsigned int pending;
};

/** Register as a name resolver */
#define __resolver __table ( struct resolver, resolvers, 01 )

extern int resolv ( const char *name, struct sockaddr *sa,
		    struct async *parent );

#endif /* _GPXE_RESOLV_H */
