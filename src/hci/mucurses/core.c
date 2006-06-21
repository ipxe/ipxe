#include <curses.h>
#include "core.h"

/** @file
 *
 * MuCurses core functions
 *
 */

WINDOW _stdscr = {
	.attrs = A_DEFAULT,
	.ori_y = 0,
	.ori_x = 0,
	.curs_y = 0,
	.curs_x = 0,
	.scr = curscr,
};

/*
 *  Primitives
 */

/**
 * Write a single character rendition to a window
 *
 * @v *win	window in which to write
 * @v ch	character rendition to write
 * @v wrap	wrap "switch"
 */
void _wputch ( WINDOW *win, chtype ch, int wrap ) {
	/* make sure we set the screen cursor to the right position
	   first! */
	win->scr->movetoyx( win->scr, win->ori_y + win->curs_y,
				      win->ori_x + win->curs_x );
	win->scr->putc(win->scr, ch);
	if ( ++(win->curs_x) - win->width == 0 ) {
		if ( wrap == WRAP ) {
			win->curs_x = 0;
			/* specification says we should really scroll,
			   but we have no buffer to scroll with, so we
			   can only overwrite back at the beginning of
			   the window */
			if ( ++(win->curs_y) - win->height == 0 )
				win->curs_y = 0;
		} else {
			(win->curs_x)--;
		}
	}
}

/**
 * Write a chtype string to a window
 *
 * @v *win	window in which to write
 * @v *chstr	chtype string
 * @v wrap	wrap "switch"
 * @v n		write at most n chtypes
 */
void _wputchstr ( WINDOW *win, const chtype *chstr, int wrap, int n ) {
	for ( ; *chstr && n-- ; chstr++ ) {
		_wputch(win,*chstr,wrap);
	}
}

/**
 * Write a standard c-style string to a window
 *
 * @v *win	window in which to write
 * @v *str	string
 * @v wrap	wrap "switch"
 * @v n		write at most n chars from *str
 */
void _wputstr ( WINDOW *win, const char *str, int wrap, int n ) {
	for ( ; *str && n-- ; str++ ) {
		_wputch( win, *str | win->attrs, wrap );
	}
}

/**
 * Move a window's cursor to the specified position
 *
 * @v *win	window to be operated on
 * @v y		Y position
 * @v x		X position
 * @ret rc	return status code
 */
int wmove ( WINDOW *win, int y, int x ) {
	/* chech for out-of-bounds errors */
	if ( ( (unsigned)y >= win->height ) ||
	     ( (unsigned)x >= win->width ) ) {
		return ERR;
	}

	win->curs_y = y;
	win->curs_x = x;
	win->scr->movetoyx( win->scr, win->ori_y + win->curs_y, 
			    	      win->ori_x + win->curs_x );
	return OK;
}
