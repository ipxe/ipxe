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
	DBGC ( intf, "INTF %p moving from INTF %p to INTF %p\n",
	       intf, intf->dest, dest );
	intf_put ( intf->dest );
	intf->dest = intf_get ( dest );
}

/**
 * Plug two interfaces together
 *
 * @v a			Interface A
 * @v b			Interface B
 *
 * Plugs interface A into interface B, and interface B into interface
 * A.  (The basic plug() function is unidirectional; this function is
 * merely a shorthand for two calls to plug(), hence the name.)
 */
void plug_plug ( struct interface *a, struct interface *b ) {
	plug ( a, b );
	plug ( b, a );
}
