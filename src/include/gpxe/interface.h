#ifndef _GPXE_INTERFACE_H
#define _GPXE_INTERFACE_H

/** @file
 *
 * Object communication interfaces
 *
 */

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
	/** Update reference count
	 *
	 * @v intf		Interface
	 * @v delta		Change to apply to reference count
	 *
	 * This method updates the reference count for the object
	 * containing the interface.
	 */
	void ( * refcnt ) ( struct interface *intf, int delta );
};

extern void plug ( struct interface *intf, struct interface *dest );

extern void null_refcnt ( struct interface *intf, int delta );

#endif /* _GPXE_INTERFACE_H */
