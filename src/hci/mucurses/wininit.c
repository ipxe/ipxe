#include <curses.h>
#include <stddef.h>
#include "core.h"

extern struct _softlabelkeys *slks;

/**
 * Initialise console environment
 *
 * @ret *win	return pointer to stdscr
 */
WINDOW *initscr ( void ) {
	/* determine console size */
	/* initialise screen */
	stdscr->width = 80;
	stdscr->height = ( slks == NULL ? 25 : 24 );
	/* set previously unknown window attributes */
	/* refresh screen */
	return stdscr;
}
