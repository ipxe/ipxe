#include <curses.h>
#include <stddef.h>
#include <timer.h>
#include "core.h"

/** @file
 *
 * MuCurses keyboard input handling functions
 */

#define INPUT_DELAY 		200 // half-blocking delay timer resolution (ms)
#define INPUT_DELAY_TIMEOUT 	1000 // half-blocking delay timeout

int m_delay; /* 
		< 0 : blocking read
		0   : non-blocking read
		> 0 : timed blocking read
	     */
bool m_echo;
bool m_cbreak;

/**
 * Check KEY_ code supported status
 *
 * @v kc	keycode value to check
 * @ret TRUE	KEY_* supported
 * @ret FALSE	KEY_* unsupported
 */
int has_key ( int kc __unused ) {
	return TRUE;
}

/**
 * Pop a character from the FIFO into a window
 *
 * @v *win	window in which to echo input
 * @ret c	char from input stream
 */
int wgetch ( WINDOW *win ) {
	int c, timer;
	if ( win == NULL )
		return ERR;

	timer = INPUT_DELAY_TIMEOUT;
	while ( ! win->scr->peek( win->scr ) ) {
		if ( m_delay == 0 ) // non-blocking read
			return ERR;
		if ( timer > 0 ) {
			if ( m_delay > 0 )
				timer -= INPUT_DELAY;
			mdelay( INPUT_DELAY );
		} else { return ERR; }
	}

	c = win->scr->getc( win->scr );

	if ( m_echo ) {
		if ( c >= 0401 && c <= 0633 ) {
			switch(c) {
			case KEY_LEFT :
			case KEY_BACKSPACE :
				if ( win->curs_x == 0 )
					wmove( win, 
					       --(win->curs_y), 
					       win->width - 1 );
				else
					wmove( win, 
					       win->curs_y, 
					       --(win->curs_x) );
				wdelch( win );
				break;
			default :
				beep();
				break;
			}
		} else {
			_wputch( win, (chtype)( c | win->attrs ), WRAP );
		}
	}

	return c;
}

/**
 * Read at most n characters from the FIFO into a window
 *
 * @v *win	window in which to echo input
 * @v *str	pointer to string in which to store result
 * @ret rc	return status code
 */
int wgetnstr ( WINDOW *win, char *str, int n ) {
	char *_str;
	int c;

	_str = str;

	while (!( n == 0 ) ) {
		c = wgetch( win );
		if ( c >= 0401 && c <= 0633 ) {
			switch(c) {
			case KEY_LEFT :
			case KEY_BACKSPACE :
				if ( _str > str ) {
					_str--; n++;
				}
				break;
			case KEY_ENTER :
				*_str = '\0';
				break;
			}
		} else if ( c == '\n' ) {
			*_str = '\0';
			break;
		}else { // *should* only be ASCII chars now
			*(_str++) = (char)c;
			n--;
		}
	}

	return OK;
}


/**
 *
 */
int echo ( void ) {
	m_echo = TRUE;
	return OK;
}

/**
 *
 */
int noecho ( void ) {
	m_echo = FALSE;
	return OK;
}
