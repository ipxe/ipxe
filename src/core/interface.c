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

#include <assert.h>
#include <gpxe/interface.h>

/** @file
 *
 * Object communication interfaces
 *
 */

/**
 * Obtain reference to interface
 *
 * @v intf		Interface
 * @ret intf		Interface
 *
 * Increases the reference count on the interface.
 */
static struct interface * intf_get ( struct interface *intf ) {
	intf->refcnt ( intf, +1 );
	return intf;
}

/**
 * Drop reference to interface
 *
 * @v intf		Interface
 *
 * Decreases the reference count on the interface.
 */
static void intf_put ( struct interface *intf ) {
	intf->refcnt ( intf, -1 );
}

/**
 * Plug an interface into a new destination interface
 *
 * @v intf		Interface
 * @v dest		New destination interface
 *
 * The reference to the existing destination interface is dropped, a
 * reference to the new destination interface is obtained, and the
 * interface is updated to point to the new destination interface.
 *
 * Note that there is no "unplug" call; instead you must plug the
 * interface into a null interface.
 */
void plug ( struct interface *intf, struct interface *dest ) {
	intf_put ( intf->dest );
	intf->dest = intf_get ( dest );
}

/**
 * Null update reference count
 *
 * @v intf		Interface
 * @v delta		Change to apply to reference count
 *
 * Use this as the refcnt() method for an interface that does not need
 * to support reference counting.
 */
void null_refcnt ( struct interface *intf __unused, int delta __unused ) {
	/* Do nothing */
}
