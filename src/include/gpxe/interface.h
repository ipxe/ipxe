#ifndef _GPXE_INTERFACE_H
#define _GPXE_INTERFACE_H

/** @file
 *
 * Object communication interfaces
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <gpxe/refcnt.h>

/** An object communication interface */
struct interface {
	/** Destination interface
	 *
	 * When messages are sent via this interface, they will be
	 * delivered to the destination interface.
	 *
	 * This pointer may never be NULL.  When the interface is
	 * unplugged, it should point to a null interface.
	 */
	struct interface *dest;
	/** Reference counter
	 *
	 * If this interface is not part of a reference-counted
	 * object, this field may be NULL.
	 */
	struct refcnt *refcnt;
};

/**
 * Increment reference count on an interface
 *
 * @v intf		Interface
 * @ret intf		Interface
 */
static inline __attribute__ (( always_inline )) struct interface *
intf_get ( struct interface *intf ) {
	ref_get ( intf->refcnt );
	return intf;
}

/**
 * Decrement reference count on an interface
 *
 * @v intf		Interface
 */
static inline __attribute__ (( always_inline )) void
intf_put ( struct interface *intf ) {
	ref_put ( intf->refcnt );
}

extern void plug ( struct interface *intf, struct interface *dest );
extern void plug_plug ( struct interface *a, struct interface *b );

#endif /* _GPXE_INTERFACE_H */
