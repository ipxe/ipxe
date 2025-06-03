/*
 * Copyright (C) 2015 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/**
 * Jump scrolling
 *
 */

#include <assert.h>
#include <ipxe/keys.h>
#include <ipxe/jumpscroll.h>

/**
 * Handle keypress
 *
 * @v scroll		Jump scroller
 * @v key		Key pressed by user
 * @ret move		Scroller movement, or zero
 */
unsigned int jump_scroll_key ( struct jump_scroller *scroll, int key ) {
	unsigned int flags = 0;
	int16_t delta;

	/* Sanity checks */
	assert ( scroll->rows != 0 );
	assert ( scroll->count != 0 );
	assert ( scroll->current < scroll->count );
	assert ( scroll->first < scroll->count );
	assert ( scroll->first <= scroll->current );
	assert ( scroll->current < ( scroll->first + scroll->rows ) );

	/* Handle key, if applicable */
	switch ( key ) {
	case KEY_UP:
		delta = -1;
		break;
	case TAB:
		flags = SCROLL_WRAP;
		/* fall through */
	case KEY_DOWN:
		delta = +1;
		break;
	case KEY_PPAGE:
		delta = ( scroll->first - scroll->current - 1 );
		break;
	case KEY_NPAGE:
		delta = ( scroll->first - scroll->current + scroll->rows );
		break;
	case KEY_HOME:
		delta = -( scroll->count );
		break;
	case KEY_END:
		delta = +( scroll->count );
		break;
	default:
		delta = 0;
		break;
	}

	return ( SCROLL ( delta ) | flags );
}

/**
 * Move scroller
 *
 * @v scroll		Jump scroller
 * @v move		Scroller movement
 * @ret move		Continuing scroller movement (if applicable)
 */
unsigned int jump_scroll_move ( struct jump_scroller *scroll,
				unsigned int move ) {
	int16_t delta = SCROLL_DELTA ( move );
	int current = scroll->current;
	int last = ( scroll->count - 1 );

	/* Sanity checks */
	assert ( move != 0 );
	assert ( scroll->count != 0 );

	/* Move to the new current item */
	current += delta;

	/* Default to continuing movement in the same direction */
	delta = ( ( delta >= 0 ) ? +1 : -1 );

	/* Check for start/end of list */
	if ( ( current >= 0 ) && ( current <= last ) ) {
		/* We are still within the list.  Update the current
		 * item and continue moving in the same direction (if
		 * applicable).
		 */
		scroll->current = current;
	} else {
		/* We have attempted to move outside the list.  If we
		 * are wrapping around, then continue in the same
		 * direction (if applicable), otherwise reverse.
		 */
		if ( ! ( move & SCROLL_WRAP ) )
			delta = -delta;

		/* Move to start or end of list as appropriate */
		if ( delta >= 0 ) {
			scroll->current = 0;
		} else {
			scroll->current = last;
		}
	}

	return ( SCROLL ( delta ) | ( move & SCROLL_FLAGS ) );
}

/**
 * Jump scroll to new page (if applicable)
 *
 * @v scroll		Jump scroller
 * @ret jumped		Jumped to a new page
 */
int jump_scroll ( struct jump_scroller *scroll ) {
	unsigned int index;

	/* Sanity checks */
	assert ( scroll->rows != 0 );
	assert ( scroll->count != 0 );
	assert ( scroll->current < scroll->count );
	assert ( scroll->first < scroll->count );

	/* Do nothing if we are already on the correct page */
	index = ( scroll->current - scroll->first );
	if ( index < scroll->rows )
		return 0;

	/* Move to required page */
	while ( scroll->first < scroll->current )
		scroll->first += scroll->rows;
	while ( scroll->first > scroll->current )
		scroll->first -= scroll->rows;

	return 1;
}
