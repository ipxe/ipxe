/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
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

/** @file
 *
 * Text widget UI
 *
 */

#include <errno.h>
#include <curses.h>
#include <ipxe/ansicol.h>
#include <ipxe/widget.h>

/**
 * Find editable widget in widget set
 *
 * @v widgets		Text widget set
 * @v index		Editable widget index
 * @ret widget		Editable widget, or NULL
 */
static struct widget * find_widget ( struct widgets *widgets,
				     unsigned int index ) {
	struct widget *widget;

	list_for_each_entry ( widget, &widgets->list, list ) {
		if ( ! ( widget->flags & WIDGET_EDITABLE ) )
			continue;
		if ( index-- == 0 )
			return widget;
	}
	return NULL;
}

/**
 * Text widget user interface main loop
 *
 * @v widgets		Text widget set
 * @ret rc		Return status code
 */
static int widget_ui_loop ( struct widgets *widgets ) {
	struct widget *widget;
	unsigned int current;
	unsigned int count;
	int key;

	/* Draw all widgets */
	list_for_each_entry ( widget, &widgets->list, list )
		draw_widget ( widgets, widget );

	/* Count editable widgets */
	count = 0;
	while ( find_widget ( widgets, count ) != NULL )
		count++;

	/* Main loop */
	current = 0;
	while ( 1 ) {

		/* Identify current widget */
		widget = find_widget ( widgets, current );
		if ( ! widget )
			return -ENOENT;

		/* Redraw current widget */
		draw_widget ( widgets, widget );

		/* Process keypress */
		key = edit_widget ( widgets, widget, getkey ( 0 ) );
		switch ( key ) {
		case KEY_UP:
			if ( current > 0 )
				current--;
			break;
		case KEY_DOWN:
			if ( ++current == count )
				current--;
			break;
		case TAB:
			if ( ++current == count )
				current = 0;
			break;
		case KEY_ENTER:
			current++;
			if ( current >= count )
				return 0;
			break;
		case CTRL_C:
		case ESC:
			return -ECANCELED;
		default:
			/* Do nothing for unrecognised keys or edit errors */
			break;
		}
	}
}

/**
 * Present text widget user interface
 *
 * @v widgets		Text widget set
 * @ret rc		Return status code
 */
int widget_ui ( struct widgets *widgets ) {
	int rc;

	/* Initialise UI */
	initscr();
	start_color();
	color_set ( CPAIR_NORMAL, NULL );
	erase();

	/* Run main loop */
	rc = widget_ui_loop ( widgets );

	/* Terminate UI */
	color_set ( CPAIR_NORMAL, NULL );
	endwin();

	return rc;
}
