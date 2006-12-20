#ifndef _GPXE_EDITBOX_H
#define _GPXE_EDITBOX_H

/** @file
 *
 * Editable text box widget
 *
 */

#include <curses.h>
#include <gpxe/editstring.h>

/** An editable text box widget */
struct edit_box {
	/** Editable string */
	struct edit_string string;
	/** Containing window */
	WINDOW *win;
	/** Row */
	unsigned int row;
	/** Starting column */
	unsigned int col;
	/** Width */
	unsigned int width;
	/** First displayed character */
	unsigned int first;
};

extern void init_editbox ( struct edit_box *box, char *buf, size_t len,
			   WINDOW *win, unsigned int row, unsigned int col,
			   unsigned int width );
extern void draw_editbox ( struct edit_box *box );
extern int edit_editbox ( struct edit_box *box, int key );

#endif /* _GPXE_EDITBOX_H */
