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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <string.h>
#include <assert.h>
#include <gpxe/editbox.h>

/** @file
 *
 * Editable text box widget
 *
 */

#define EDITBOX_MIN_CHARS 3

/**
 * Initialise text box widget
 *
 * @v box		Editable text box widget
 * @v buf		Text buffer
 * @v len		Size of text buffer
 * @v win		Containing window
 * @v row		Row
 * @v col		Starting column
 * @v width		Width
 *
 */
void init_editbox ( struct edit_box *box, char *buf, size_t len,
		    WINDOW *win, unsigned int row, unsigned int col,
		    unsigned int width ) {
	memset ( box, 0, sizeof ( *box ) );
	box->string.buf = buf;
	box->string.len = len;
	box->string.cursor = strlen ( buf );
	box->win = ( win ? win : stdscr );
	box->row = row;
	box->col = col;
	box->width = width;
}

/**
 * Draw text box widget
 *
 * @v box		Editable text box widget
 *
 */
void draw_editbox ( struct edit_box *box ) {
	size_t width = box->width;
	char buf[ width + 1 ];
	size_t keep_len;
	signed int cursor_offset, underflow, overflow;
	size_t len;

	/* Adjust starting offset so that cursor remains within box */
	cursor_offset = ( box->string.cursor - box->first );
	keep_len = strlen ( box->string.buf );
	if ( keep_len > EDITBOX_MIN_CHARS )
		keep_len = EDITBOX_MIN_CHARS;
	underflow = ( keep_len - cursor_offset );
	overflow = ( cursor_offset - ( width - 1 ) );
	if ( underflow > 0 ) {
		box->first -= underflow;
	} else if ( overflow > 0 ) {
		box->first += overflow;
	}
	cursor_offset = ( box->string.cursor - box->first );

	/* Construct underscore-padded string portion */
	memset ( buf, '_', width );
	buf[width] = '\0';
	len = ( strlen ( box->string.buf ) - box->first );
	if ( len > width )
		len = width;
	memcpy ( buf, ( box->string.buf + box->first ), len );

	/* Print box content and move cursor */
	if ( ! box->win )
		box->win = stdscr;
	mvwprintw ( box->win, box->row, box->col, "%s", buf );
	wmove ( box->win, box->row, ( box->col + cursor_offset ) );
}

/**
 * Edit text box widget
 *
 * @v box		Editable text box widget
 * @v key		Key pressed by user
 * @ret key		Key returned to application, or zero
 *
 */
int edit_editbox ( struct edit_box *box, int key ) {

	/* Update the string itself */
	key = edit_string ( &box->string, key );

	/* Update the display */
	draw_editbox ( box );

	return key;
}
