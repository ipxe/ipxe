/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <string.h>
#include <assert.h>
#include <ipxe/ansicol.h>
#include <ipxe/editbox.h>

/** @file
 *
 * Editable text box widget
 *
 */

#define EDITBOX_MIN_CHARS 3

/**
 * Draw text box widget
 *
 * @v widget		Text widget
 */
static void draw_editbox ( struct widget *widget ) {
	struct edit_box *box = container_of ( widget, struct edit_box, widget );
	const char *content = *(box->string.buf);
	size_t width = widget->width;
	char buf[ width + 1 ];
	signed int cursor_offset, underflow, overflow, first;
	size_t len;

	/* Adjust starting offset so that cursor remains within box */
	cursor_offset = ( box->string.cursor - box->first );
	underflow = ( EDITBOX_MIN_CHARS - cursor_offset );
	overflow = ( cursor_offset - ( width - 1 ) );
	first = box->first;
	if ( underflow > 0 ) {
		first -= underflow;
		if ( first < 0 )
			first = 0;
	} else if ( overflow > 0 ) {
		first += overflow;
	}
	box->first = first;
	cursor_offset = ( box->string.cursor - first );

	/* Construct underscore-padded string portion */
	memset ( buf, '_', width );
	buf[width] = '\0';
	len = ( content ? ( strlen ( content ) - first ) : 0 );
	if ( len > width )
		len = width;
	if ( widget->flags & WIDGET_SECRET ) {
		memset ( buf, '*', len );
	} else {
		memcpy ( buf, ( content + first ), len );
	}

	/* Print box content and move cursor */
	color_set ( CPAIR_EDIT, NULL );
	mvprintw ( widget->row, widget->col, "%s", buf );
	move ( widget->row, ( widget->col + cursor_offset ) );
	color_set ( CPAIR_NORMAL, NULL );
}

/**
 * Edit text box widget
 *
 * @v widget		Text widget
 * @v key		Key pressed by user
 * @ret key		Key returned to application, or zero
 */
static int edit_editbox ( struct widget *widget, int key ) {
	struct edit_box *box = container_of ( widget, struct edit_box, widget );

	return edit_string ( &box->string, key );
}

/** Text box widget operations */
struct widget_operations editbox_operations = {
	.draw = draw_editbox,
	.edit = edit_editbox,
};
