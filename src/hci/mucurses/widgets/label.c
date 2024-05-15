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

#include <string.h>
#include <assert.h>
#include <ipxe/label.h>

/** @file
 *
 * Text label widget
 *
 */

/**
 * Draw text label widget
 *
 * @v widgets		Text widget set
 * @v widget		Text widget
 */
static void draw_label ( struct widgets *widgets, struct widget *widget ) {
	struct label *label = container_of ( widget, struct label, widget );
	unsigned int width = widget->width;
	unsigned int col = widget->col;
	const char *text = label->text;

	/* Centre label if width is non-zero */
	if ( width )
		col += ( ( width - strlen ( text ) ) / 2 );

	/* Print label content */
	attron ( A_BOLD );
	mvwprintw ( widgets->win, widget->row, col, "%s", text );
	attroff ( A_BOLD );
}

/**
 * Edit text label widget
 *
 * @v widgets		Text widget set
 * @v widget		Text widget
 * @v key		Key pressed by user
 * @ret key		Key returned to application, or zero
 */
static int edit_label ( struct widgets *widgets __unused,
			struct widget *widget __unused, int key ) {

	/* Cannot be edited */
	return key;
}

/** Text label widget operations */
struct widget_operations label_operations = {
	.draw = draw_label,
	.edit = edit_label,
};
