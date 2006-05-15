#include <curses.h>
#include <malloc.h>
#include <stddef.h>

/** @file
 *
 * MuCurses: lightweight xcurses implementation for PXE ROMs
 *
 */

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
	_putc( win->scr, ch & A_CHARTEXT );
	_advcurs_wrap( win );
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
	unsigned int ch, pos, count = 0;
	chtype *chptr = chstr;

	pos = _store_curs_pos ( win );
	while ( ( ( ( ch = ( *chptr & A_CHARTEXT ) ) ) != '\0' )
		&& ( count++ < (unsigned)n ) ) {
		_putc( win, ch );
		_advcurs_nowrap( win );
		/* set rendition code here */
	}
	_restore_curs_pos( win, pos ) && return OK;
 err:
	_restore_curs_pos( win, pos ) && return ERR;
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
		_putc( win, ch );
		_advcurs_wrap( win );
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
 * Set background rendition attributes for a window and apply to
 * contents
 *
 * @v *win	window to be operated on
 * @v ch	chtype containing rendition attributes
 * @ret rc	return status code
 */
int wbkgd ( WINDOW *win, chtype ch ) {
	return OK;
 err:
	return ERR;
}

/**
 * Set background rendition attributes for a window
 *
 * @v *win	window to be operated on
 * @v ch	chtype containing rendition attributes
 */
void wbkgdset ( WINDOW *win, chtype ch ) {
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

/**
 * Move a window's cursor to the specified position
 *
 * @v *win	window to be operated on
 * @v y		Y position
 * @v x		X position
 * @ret rc	return status code
 */
int wmove ( WINDOW *win, int y, int x ) {
	_movetoyx( win->scr, y, x );
	return OK;
 err:
	return ERR;
}
