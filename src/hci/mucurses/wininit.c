#include <curses.h>

/** @file
 *
 * MuCurses initialisation functions
 *
 */

/**
 * Initialise console environment
 *
 * @ret *win	return pointer to stdscr
 */
WINDOW *initscr ( void ) {
	/* determine console size */
	/* initialise screen */
	curscr->init( curscr );
	stdscr->height = LINES;
	stdscr->width = COLS;
	werase( stdscr );

	return stdscr;
}
