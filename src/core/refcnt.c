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

#include <stdlib.h>
#include <gpxe/refcnt.h>

/** @file
 *
 * Reference counting
 *
 */

/**
 * Increment reference count
 *
 * @v refcnt		Reference counter, or NULL
 * @ret refcnt		Reference counter
 *
 * If @c refcnt is NULL, no action is taken.
 */
struct refcnt * ref_get ( struct refcnt *refcnt ) {

	if ( refcnt ) {
		refcnt->refcnt++;
		DBGC2 ( refcnt, "REFCNT %p incremented to %d\n",
			refcnt, refcnt->refcnt );
	}
	return refcnt;
}

/**
 * Decrement reference count
 *
 * @v refcnt		Reference counter, or NULL
 *
 * If the reference count decreases below zero, the object's free()
 * method will be called.
 *
 * If @c refcnt is NULL, no action is taken.
 */
void ref_put ( struct refcnt *refcnt ) {

	if ( ! refcnt )
		return;

	refcnt->refcnt--;
	DBGC2 ( refcnt, "REFCNT %p decremented to %d\n",
		refcnt, refcnt->refcnt );

	if ( refcnt->refcnt >= 0 )
		return;

	if ( refcnt->free ) {
		DBGC ( refcnt, "REFCNT %p being freed via method %p\n",
		       refcnt, refcnt->free );
		refcnt->free ( refcnt );
	} else {
		DBGC ( refcnt, "REFCNT %p being freed\n", refcnt );
		free ( refcnt );
	}
}
