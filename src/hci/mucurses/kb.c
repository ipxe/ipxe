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

int _wgetc ( WINDOW *win ) {
	int timer, c;

	if ( win == NULL )
		return ERR;

	timer = INPUT_DELAY_TIMEOUT;
	while ( ! win->scr->peek( win->scr ) ) {
		if ( m_delay == 0 ) // non-blocking read
			return ERR;
		if ( timer > 0 ) {  // time-limited blocking read
			if ( m_delay > 0 )
				timer -= INPUT_DELAY;
			mdelay( INPUT_DELAY );
		} else { return ERR; } // non-blocking read
	}

	c = win->scr->getc( win->scr );

	if ( m_echo && ( c >= 32 && c <= 126 ) ) // printable ASCII characters
		_wputch( win, (chtype) ( c | win->attrs ), WRAP );

	return c;
}

/**
 * Pop a character from the FIFO into a window
 *
 * @v *win	window in which to echo input
 * @ret c	char from input stream
 */
int wgetch ( WINDOW *win ) {
	int c;

	c = _wgetc( win );

	if ( m_echo ) {
		if ( c >= 0401 && c <= 0633 ) {
			switch(c) {
			case KEY_LEFT :
			case KEY_BACKSPACE :
				_wcursback( win );
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
 * @v n		maximum number of characters to read into string (inc. NUL)
 * @ret rc	return status code
 */
int wgetnstr ( WINDOW *win, char *str, int n ) {
	char *_str;
	int c;

	if ( n == 0 ) {
		str = '\0';
		return OK;
	}

	_str = str;

	while ( ( c = _wgetc( win ) ) != ERR ) {
		/* termination enforcement - don't let us go past the
		   end of the allocated buffer... */
		if ( n == 0 && ( c >= 32 && c <= 126 ) ) {
			_wcursback( win );
			wdelch( win );
		} else {
			if ( c >= 0401 && c <= 0633 ) {
				switch(c) {
				case KEY_LEFT :
				case KEY_BACKSPACE :
					_wcursback( win );
					wdelch( win );
					break;
				case KEY_ENTER :
					*_str = '\0';
					return OK;
				default :
					beep();
					break;
				}
			}
			if ( c >= 32 && c <= 126 ) {
				*(_str++) = c; n--;
			}
		}
	}

	return ERR;
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
