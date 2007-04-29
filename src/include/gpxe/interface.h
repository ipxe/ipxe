#ifndef _GPXE_INTERFACE_H
#define _GPXE_INTERFACE_H

/** @file
 *
 * Object communication interfaces
 *
 */

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

extern void plug ( struct interface *intf, struct interface *dest );

#endif /* _GPXE_INTERFACE_H */
