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
	stdscr->scr->init( stdscr->scr );
	stdscr->height = LINES;
	stdscr->width = COLS;
	werase( stdscr );

	return stdscr;
}

/**
 * Finalise console environment
 *
 */
int endwin ( void ) {
	stdscr->scr->exit( stdscr->scr );
	return OK;
}
