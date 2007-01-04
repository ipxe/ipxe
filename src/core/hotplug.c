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
#include <gpxe/hotplug.h>

/** @file
 *
 * Hotplug support
 *
 */

/**
 * Forget all persistent references to an object
 *
 * @v list		List of persistent references
 */
void forget_references ( struct list_head *list ) {
	struct reference *ref;
	struct reference *ref_tmp;

	list_for_each_entry_safe ( ref, ref_tmp, list, list ) {
		ref->forget ( ref );
	}

	/* The list had really better be empty by now, otherwise we're
	 * screwed.
	 */
	assert ( list_empty ( list ) );
}
