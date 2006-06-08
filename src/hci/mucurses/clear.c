#include <curses.h>
#include "core.h"
#include "cursor.h"

/**
 * Clear a window to the bottom from current cursor position
 *
 * @v *win	subject window
 * @ret rc	return status code
 */
int wclrtobot ( WINDOW *win ) {
	struct cursor_pos pos;

	_store_curs_pos( win, &pos );
	do {
		_wputch( win, (unsigned)' ', WRAP );
	} while ( win->curs_y + win->curs_x );
	_restore_curs_pos( win, &pos );

	return OK;
}

/**
 * Clear a window to the end of the current line
 *
 * @v *win	subject window
 * @ret rc	return status code
 */
int wclrtoeol ( WINDOW *win ) {
	struct cursor_pos pos;

	_store_curs_pos( win, &pos );
	while ( ( win->curs_y - pos.y ) == 0 ) {
		_wputch( win, (unsigned)' ', WRAP );
	}
	_restore_curs_pos( win, &pos );

	return OK;
}

/**
 * Completely clear a window
 *
 * @v *win	subject window
 * @ret rc	return status code
 */
int werase ( WINDOW *win ) {
	wmove( win, 0, 0 );
	wclrtobot( win );
	return OK;
}
