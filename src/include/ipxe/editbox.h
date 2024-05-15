#ifndef _IPXE_EDITBOX_H
#define _IPXE_EDITBOX_H

/** @file
 *
 * Editable text box widget
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <curses.h>
#include <ipxe/editstring.h>
#include <ipxe/widget.h>

/** An editable text box widget */
struct edit_box {
	/** Text widget */
	struct widget widget;
	/** Editable string */
	struct edit_string string;
	/** First displayed character */
	unsigned int first;
};

extern struct widget_operations editbox_operations;

/**
 * Initialise text box widget
 *
 * @v box		Editable text box widget
 * @v row		Row
 * @v col		Starting column
 * @v width		Width
 * @v flags		Flags
 * @v buf		Dynamically allocated string buffer
 */
static inline __attribute__ (( always_inline )) void
init_editbox ( struct edit_box *box, unsigned int row, unsigned int col,
	       unsigned int width, unsigned int flags, char **buf ) {

	init_widget ( &box->widget, &editbox_operations, row, col,
		      width, ( flags | WIDGET_EDITABLE ) );
	init_editstring ( &box->string, buf );
	if ( *buf )
		box->string.cursor = strlen ( *buf );
}

#endif /* _IPXE_EDITBOX_H */
