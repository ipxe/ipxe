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
	stdscr->height = LINES;
	stdscr->width = COLS;
	/* set previously unknown window attributes */
	/* refresh screen */
	return stdscr;
}
