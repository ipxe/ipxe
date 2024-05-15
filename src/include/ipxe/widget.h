#ifndef _IPXE_WIDGET_H
#define _IPXE_WIDGET_H

/** @file
 *
 * Text widgets
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <curses.h>
#include <ipxe/list.h>

/** A text widget set */
struct widgets {
	/** List of widgets (in tab order) */
	struct list_head list;
	/** Containing window */
	WINDOW *win;
};

/** A text widget */
struct widget {
	/** List of widgets (in tab order) */
	struct list_head list;
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
	 * @v widgets		Text widget set
	 * @v widget		Text widget
	 */
	void ( * draw ) ( struct widgets *widgets, struct widget *widget );
	/**
	 * Edit widget
	 *
	 * @v widgets		Text widget set
	 * @v widget		Text widget
	 * @v key		Key pressed by user
	 * @ret key		Key returned to application, or zero
	 *
	 * This will not update the display: you must call the draw()
	 * method to ensure that any changes to an editable widget are
	 * displayed to the user.
	 */
	int ( * edit ) ( struct widgets *widgets, struct widget *widget,
			 int key );
};

/**
 * Initialise text widget set
 *
 * @v widgets		Text widget set
 * @v win		Containing window
 */
static inline __attribute__ (( always_inline )) void
init_widgets ( struct widgets *widgets, WINDOW *win ) {

	INIT_LIST_HEAD ( &widgets->list );
	widgets->win = ( win ? win : stdscr );
}

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
 * Append text widget
 *
 * @v widgets		Text widget set
 * @v widget		Text widget
 */
static inline __attribute__ (( always_inline )) void
add_widget ( struct widgets *widgets, struct widget *widget ) {

	list_add_tail ( &widget->list, &widgets->list );
}

/**
 * Draw text widget
 *
 * @v widgets		Text widget set
 * @v widget		Text widget
 */
static inline __attribute__ (( always_inline )) void
draw_widget ( struct widgets *widgets, struct widget *widget ) {

	widget->op->draw ( widgets, widget );
}

/**
 * Edit text widget
 *
 * @v widgets		Text widget set
 * @v widget		Text widget
 * @v key		Key pressed by user
 * @ret key		Key returned to application, or zero
 *
 * This will not update the display: you must call draw_widget() to
 * ensure that any changes to an editable widget are displayed to the
 * user.
 */
static inline __attribute__ (( always_inline )) int
edit_widget ( struct widgets *widgets, struct widget *widget, int key ) {

	return widget->op->edit ( widgets, widget, key );
}

extern int widget_ui ( struct widgets *widgets );

#endif /* _IPXE_WIDGET_H */
