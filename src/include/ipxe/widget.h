#ifndef _IPXE_WIDGET_H
#define _IPXE_WIDGET_H

/** @file
 *
 * Text widgets
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <curses.h>

/** A text widget */
struct widget {
	/** Widget operations */
	struct widget_operations *op;

	/** Row */
	unsigned int row;
	/** Starting column */
	unsigned int col;
	/** Width */
	unsigned int width;
	/** Flags */
	unsigned int flags;
};

/** Text widget flags */
enum widget_flags {
	/** Widget may have input focus */
	WIDGET_EDITABLE = 0x0001,
	/** Widget contains a secret */
	WIDGET_SECRET = 0x0002,
};

/** Text widget operations */
struct widget_operations {
	/**
	 * Draw widget
	 *
	 * @v widget		Text widget
	 */
	void ( * draw ) ( struct widget *widget );
	/**
	 * Edit widget
	 *
	 * @v widget		Text widget
	 * @v key		Key pressed by user
	 * @ret key		Key returned to application, or zero
	 *
	 * This will not update the display: you must call the draw()
	 * method to ensure that any changes to an editable widget are
	 * displayed to the user.
	 */
	int ( * edit ) ( struct widget *widget, int key );
};

/**
 * Initialise text widget
 *
 * @v widget		Text widget
 * @v op		Text widget operations
 * @v row		Row
 * @v col		Starting column
 * @v width		Width
 */
static inline __attribute__ (( always_inline )) void
init_widget ( struct widget *widget, struct widget_operations *op,
	      unsigned int row, unsigned int col, unsigned int width,
	      unsigned int flags ) {

	widget->op = op;
	widget->row = row;
	widget->col = col;
	widget->width = width;
	widget->flags = flags;
}

/**
 * Draw text widget
 *
 * @v widget		Text widget
 */
static inline __attribute__ (( always_inline )) void
draw_widget ( struct widget *widget ) {

	widget->op->draw ( widget );
}

/**
 * Edit text widget
 *
 * @v widget		Text widget
 * @v key		Key pressed by user
 * @ret key		Key returned to application, or zero
 *
 * This will not update the display: you must call draw_widget() to
 * ensure that any changes to an editable widget are displayed to the
 * user.
 */
static inline __attribute__ (( always_inline )) int
edit_widget ( struct widget *widget, int key ) {

	return widget->op->edit ( widget, key );
}

#endif /* _IPXE_WIDGET_H */
