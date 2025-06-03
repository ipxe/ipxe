#ifndef _IPXE_LABEL_H
#define _IPXE_LABEL_H

/** @file
 *
 * Text label widget
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <curses.h>
#include <ipxe/widget.h>

/** A text label widget */
struct label {
	/** Text widget */
	struct widget widget;
	/** Label text */
	const char *text;
};

extern struct widget_operations label_operations;

/**
 * Initialise text label widget
 *
 * @v label		Text label widget
 * @v row		Row
 * @v col		Starting column
 * @v width		Width
 * @v text		Label text
 */
static inline __attribute__ (( always_inline )) void
init_label ( struct label *label, unsigned int row, unsigned int col,
	     unsigned int width, const char *text ) {

	init_widget ( &label->widget, &label_operations, row, col, width, 0 );
	label->text = text;
}

#endif /* _IPXE_LABEL_H */
