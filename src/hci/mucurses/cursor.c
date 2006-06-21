#include <curses.h>
#include "cursor.h"

/** @file
 *
 * MuCurses cursor preserving functions
 *
 */

/**
 * Restore cursor position from encoded backup variable
 *
 * @v *win	window on which to operate
 * @v *pos	pointer to struct in which original cursor position is stored
 */
void _restore_curs_pos ( WINDOW *win, struct cursor_pos *pos ) {
	win->curs_y = pos->y;
	win->curs_x = pos->x;
	win->scr->movetoyx ( win->scr, win->curs_y, win->curs_x );
}

/**
 * Store cursor position for later restoration
 *
 * @v *win	window on which to operate
 * @v *pos	pointer to struct in which to store cursor position
 */
void _store_curs_pos ( WINDOW *win, struct cursor_pos *pos ) {
	pos->y = win->curs_y;
	pos->x = win->curs_x;
}
