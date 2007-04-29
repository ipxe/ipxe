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

#include <gpxe/interface.h>

/** @file
 *
 * Object communication interfaces
 *
 */

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
	ref_put ( intf->refcnt );
	ref_get ( dest->refcnt );
	intf->dest = dest;
}
