#include <curses.h>
#include <stddef.h>
#include "core.h"

/** @file
 *
 * MuCurses keyboard input handling functions
 */

#define INPUT_BUFFER_LEN 80

bool echo_on = FALSE;
bool delay = FALSE;

/**
 *
 */
int has_key ( int ch ) {
	return TRUE;
}

/**
 * Push a character back onto the FIFO
 *
 * @v ch	char to push to head of input stream
 * @ret rc	return status code
 */
int ungetch ( int ch ) {
	stdscr->scr->pushc( stdscr->scr, ch );
	return OK;
}

/**
 * Pop a character from the FIFO into a window
 *
 * @v *win	window in which to echo input
 * @ret ch	char from input stream
 */
int wgetch ( WINDOW *win ) {
	int ch;
	if ( win == NULL )
		return ERR;

	ch = win->scr->popc( win->scr );

	if ( echo_on ) {
		if ( ch == KEY_LEFT || ch == KEY_BACKSPACE ) {
			if ( win->curs_x == 0 ) {
				wmove( win, --(win->curs_y), win->width - 1 );
				wdelch( win );
			} else {
				wmove( win, win->curs_y, --(win->curs_x) );
				wdelch( win );
			}
		} else if ( ch >= 0401 && ch <= 0633 ) {
			beep();
		} else {
			_wputch( win, (chtype)( ch | win->attrs ), WRAP );
		}
	}

	return ch;
}

/**
 * Read at most n characters from the FIFO into a window
 *
 * @v *win	window in which to echo input
 * @v *str	pointer to string in which to store result
 */
int wgetnstr ( WINDOW *win, char *str, int n ) {
	char *str_start;
	int c;

	if ( n < 0 )
		return ERR;

	str_start = str;

	for ( ; ( c = wgetch(win) ) && n; n-- ) {
		if ( n == 1 ) { // last character must be a newline...
			if ( c == '\n' ) {
				*str = '\0';
			} else { // ...otherwise beep and wait for one
				beep();
				++n;
				continue;
			}
		} else if ( c == '\n' ) {
			*str = '\0';
			break;
		} else {
			if ( c == KEY_LEFT || c == KEY_BACKSPACE ) {
				if ( ! ( str == str_start ) )
					str--;
			} else { *str = c; str++; }
		}
	}

	return OK;
}
