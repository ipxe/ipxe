#ifndef _GPXE_RESOLV_H
#define _GPXE_RESOLV_H

/** @file
 *
 * Name resolution
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <gpxe/refcnt.h>
#include <gpxe/interface.h>
#include <gpxe/tables.h>
#include <gpxe/socket.h>

struct resolv_interface;

/** Name resolution interface operations */
struct resolv_interface_operations {
	/** Name resolution completed
	 *
	 * @v resolv		Name resolution interface
	 * @v sa		Completed socket address (if successful)
	 * @v rc		Final status code
	 */
	void ( * done ) ( struct resolv_interface *resolv,
			  struct sockaddr *sa, int rc );
};

/** A name resolution interface */
struct resolv_interface {
	/** Generic object communication interface */
	struct interface intf;
	/** Operations for received messages */
	struct resolv_interface_operations *op;
};

extern struct resolv_interface null_resolv;
extern struct resolv_interface_operations null_resolv_ops;

/**
 * Initialise a name resolution interface
 *
 * @v resolv		Name resolution interface
 * @v op		Name resolution interface operations
 * @v refcnt		Containing object reference counter, or NULL
 */
static inline void resolv_init ( struct resolv_interface *resolv,
				 struct resolv_interface_operations *op,
				 struct refcnt *refcnt ) {
	resolv->intf.dest = &null_resolv.intf;
	resolv->intf.refcnt = refcnt;
	resolv->op = op;
}

/**
 * Get name resolution interface from generic object communication interface
 *
 * @v intf		Generic object communication interface
 * @ret resolv		Name resolution interface
 */
static inline __attribute__ (( always_inline )) struct resolv_interface *
intf_to_resolv ( struct interface *intf ) {
	return container_of ( intf, struct resolv_interface, intf );
}

/**
 * Get reference to destination name resolution interface
 *
 * @v resolv		Name resolution interface
 * @ret dest		Destination interface
 */
static inline __attribute__ (( always_inline )) struct resolv_interface *
resolv_get_dest ( struct resolv_interface *resolv ) {
	return intf_to_resolv ( intf_get ( resolv->intf.dest ) );
}

/**
 * Drop reference to name resolution interface
 *
 * @v resolv		name resolution interface
 */
static inline __attribute__ (( always_inline )) void
resolv_put ( struct resolv_interface *resolv ) {
	intf_put ( &resolv->intf );
}

/**
 * Plug a name resolution interface into a new destination interface
 *
 * @v resolv		Name resolution interface
 * @v dest		New destination interface
 */
static inline __attribute__ (( always_inline )) void
resolv_plug ( struct resolv_interface *resolv, struct resolv_interface *dest ) {
	plug ( &resolv->intf, &dest->intf );
}

/**
 * Plug two name resolution interfaces together
 *
 * @v a			Name resolution interface A
 * @v b			Name resolution interface B
 */
static inline __attribute__ (( always_inline )) void
resolv_plug_plug ( struct resolv_interface *a, struct resolv_interface *b ) {
	plug_plug ( &a->intf, &b->intf );
}

/**
 * Unplug a name resolution interface
 *
 * @v resolv		Name resolution interface
 */
static inline __attribute__ (( always_inline )) void
resolv_unplug ( struct resolv_interface *resolv ) {
	plug ( &resolv->intf, &null_resolv.intf );
}

/**
 * Stop using a name resolution interface
 *
 * @v resolv		Name resolution interface
 *
 * After calling this method, no further messages will be received via
 * the interface.
 */
static inline void resolv_nullify ( struct resolv_interface *resolv ) {
	resolv->op = &null_resolv_ops;
};

/** A name resolver */
struct resolver {
	/** Name of this resolver (e.g. "DNS") */
	const char *name;
	/** Start name resolution
	 *
	 * @v resolv		Name resolution interface
	 * @v name		Name to resolve
	 * @v sa		Socket address to complete
	 * @ret rc		Return status code
	 */
	int ( * resolv ) ( struct resolv_interface *resolv, const char *name,
			   struct sockaddr *sa );
};

/** Numeric resolver priority */
#define RESOLV_NUMERIC 01

/** Normal resolver priority */
#define RESOLV_NORMAL 02

/** Resolvers table */
#define RESOLVERS __table ( struct resolver, "resolvers" )

/** Register as a name resolver */
#define __resolver( resolv_order ) __table_entry ( RESOLVERS, resolv_order )

extern void resolv_done ( struct resolv_interface *resolv,
			  struct sockaddr *sa, int rc );
extern void ignore_resolv_done ( struct resolv_interface *resolv,
			  struct sockaddr *sa, int rc );
extern struct resolv_interface_operations null_resolv_ops;
extern struct resolv_interface null_resolv;

extern int resolv ( struct resolv_interface *resolv, const char *name,
		    struct sockaddr *sa );

#endif /* _GPXE_RESOLV_H */
