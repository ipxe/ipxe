#include <curses.h>
#include <malloc.h>
#include <stddef.h>

/** @file
 *
 * MuCurses: lightweight xcurses implementation for PXE ROMs
 *
 */

#define WRAP 0
#define NOWRAP 1

struct cursor_pos {
	unsigned int y, x;
};

/*
  Primitives
*/

/**
 * Write a single character rendition to a window
 *
 * @v *win	window in which to write
 * @v ch	character rendition to write
 */
static void _wputch ( WINDOW *win, chtype ch, int wrap ) {
	win->scr->putc(win->scr, ch);
	if ( ++(win->curs_x) > win->width ) {
		if ( wrap == WRAP ) {
			win->curs_x = 0;
			(win->curs_y)++;
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
static void _wputchstr ( WINDOW *win, chtype *chstr, int wrap, int n ) {
	for ( ; *chstr && n-- ; chstr++ ) {
		_wputch(win,*chstr,wrap);
	}
}

/**
 * Write a standard c-style string to a window
 * @v *win	window in which to write
 * @v *str	string
 * @v wrap	wrap "switch"
 * @v n		write at most n chars from *str
 */
static void _wputstr ( WINDOW *win, char *str, int wrap, int n ) {
	for ( ; *str && n-- ; str++ ) {
		_wputch( win, *str | win->attrs, wrap );
	}
}

/**
 * Restore cursor position from encoded backup variable
 *
 * @v *win	window on which to operate
 * @v *pos	pointer to struct in which original cursor position is stored
 */
static void _restore_curs_pos ( WINDOW *win, struct cursor_pos *pos ){
	win->curs_y = pos->y;
	win->curs_x = pos->x;
}

/**
 * Store cursor position for later restoration
 *
 * @v *win	window on which to operate
 * @v *pos	pointer to struct in which to store cursor position
 */
static void _store_curs_pos ( WINDOW *win, struct cursor_pos *pos ) {
	pos->y = win->curs_y;
	pos->x = win->curs_x;
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
	if ( ( ( (unsigned)x - win->ori_x ) > win->width ) ||
	     ( ( (unsigned)y - win->ori_y ) > win->height ) ) {
		return ERR;
	}

	win->scr->movetoyx( win->scr, y, x );
	return OK;
}




WINDOW _stdscr = {
	.attrs = A_DEFAULT,
	.ori_y = 0,
	.ori_x = 0,
	.curs_y = 0,
	.curs_x = 0,
};

/**
 * get terminal baud rate
 *
 * @ret bps	return baud rate in bits per second
 */
int baudrate ( void ) {
	return 0;
}

/**
 * Audible (or visual) signal
 *
 * @ret rc	return status code
 */
int beep ( void ) {
	/* ok, so I can't waste memory buffering the screen (or in
	   this case, backing up the background colours of the screen
	   elements), but maybe I can buffer the border and flash that
	   - or maybe even just the top and bottom? Assuming I can't
	   make the system speaker beep, of course... */
	return OK;
}

/**
 * Draw borders from single-byte characters and renditions around a
 * window
 *
 * @v *win	window to be bordered
 * @v verch	vertical chtype
 * @v horch	horizontal chtype
 * @ret rc	return status code
 */
int box ( WINDOW *win, chtype verch, chtype horch ) {
	return OK;
 err:
	return ERR;
}

/**
 * Indicates whether the attached terminal is capable of having
 * colours redefined
 *
 * @ret bool	returns boolean dependent on colour changing caps of terminal
 */
bool can_change_colour ( void ) {
	return (bool)TRUE;
 err:
	return (bool)FALSE;
}

/**
 * Identifies the intensity components of colour number "colour" and
 * stores the RGB intensity values in the respective addresses pointed
 * to by "red", "green" and "blue" respectively
 */
int colour_content ( short colour, short *red, short *green, short *blue ) {
	return OK;
 err:
	return ERR;
}

/**
 * Window colour attribute control function
 *
 * @v colour_pair_number	colour pair integer
 * @v *opts			pointer to options
 * @ret rc			return status code
 */
int colour_set ( short colour_pair_number, void *opts ) {
	return OK;
}

/**
 * Delete a window
 *
 * @v *win	pointer to window being deleted
 * @ret rc	return status code
 */
int delwin ( WINDOW *win ) {
	if ( win == NULL )
		goto err;
	/* must free descendants first, but I haven't implemented descendants yet
	   ... */
	free(win);
	return OK;
 err:
	return ERR;
}

/**
 * Initialise console environment
 *
 * @ret *win	return pointer to stdscr
 */
WINDOW *initscr ( void ) {
	/* determine console size */
	/* initialise screen */
	/* set previously unknown window attributes */
	/* refresh screen */
	return stdscr;
}

/**
 * Create new WINDOW
 *
 * @v nlines	number of lines
 * @v ncols	number of columns
 * @v begin_y	column origin
 * @v begin_x	line origin
 * @ret *win	return pointer to new window
 */
WINDOW *newwin ( int nlines, int ncols, int begin_y, int begin_x ) {
	WINDOW *win = calloc( 1, sizeof(WINDOW) );
	win->ori_y = begin_y;
	win->ori_x = begin_x;
	win->height = nlines;
	win->width = ncols;
	win->scr = stdscr->scr;
	return win;
}

/**
 * Add a single-byte character and rendition to a window and advance
 * the cursor
 *
 * @v *win	window to be rendered in
 * @v ch	character to be added at cursor
 * @ret rc	return status code
 */
int waddch ( WINDOW *win, const chtype ch ) {
	return OK;
 err:
	return ERR;
}

/**
 * Add string of single-byte characters and renditions to a window
 *
 * @v *win	window to be rendered in
 * @v *chstr	pointer to first chtype in "string"
 * @v n		max number of chars from chstr to render
 * @ret rc	return status code
 */
int waddchnstr ( WINDOW *win, const chtype *chstr, int n ) {
	struct cursor_pos pos;	

	_store_curs_pos( win, &pos );
	_wputchstr( win, chstr, NOWRAP, n );
	_restore_curs_pos( win, &pos );
	return OK;
}

/**
 * Add string of single-byte characters to a window
 *
 * @v *win	window to be rendered in
 * @v *str	standard c-style string
 * @v n		max number of chars from string to render
 * @ret rc	return status code
 */
int waddnstr ( WINDOW *win, const char *str, int n ) {
	unsigned int ch, count = 0;
	char *strptr = str;

	while ( ( ( ch = *strptr ) != '\0' )
		&& ( count++ < (unsigned)n ) ) {
	}

	return OK;
 err:
	return ERR;
}

/**
 * Turn off attributes
 *
 * @v win	subject window
 * @v attrs	attributes to enable
 * @ret rc	return status code
 */
int wattroff ( WINDOW *win, int attrs ) {
	win->attrs &= ~attrs;
	return 0;
}

/**
 * Turn on attributes
 *
 * @v win	subject window
 * @v attrs	attributes to enable
 * @ret rc	return status code
 */
int wattron ( WINDOW *win, int attrs ) {
	win->attrs |= attrs;
	return OK;
}

/**
 * Set attributes
 *
 * @v win	subject window
 * @v attrs	attributes to enable
 * @ret rc	return status code
 */
int wattrset ( WINDOW *win, int attrs ) {
	win->attrs = attrs;
	return OK;
}

/**
 * Draw borders from single-byte characters and renditions around a
 * window
 *
 * @v *win	window to be bordered
 * @v ls	left side
 * @v rs	right side
 * @v ts	top
 * @v bs	bottom
 * @v tl	top left corner
 * @v tr	top right corner
 * @v bl	bottom left corner
 * @v br	bottom right corner
 * @ret rc	return status code
 */
int wborder ( WINDOW *win, chtype ls, chtype rs,
	      chtype ts, chtype bs, chtype tl,
	      chtype tr, chtype bl, chtype br ) {
	return OK;
 err:
	return ERR;
}

