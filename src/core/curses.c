#include <curses.h>
#include <malloc.h>
#include <stddef.h>
#include <vsprintf.h>

/** @file
 *
 * MuCurses: lightweight xcurses implementation for PXE ROMs
 *
 */

#define WRAP 0
#define NOWRAP 1

unsigned short _COLS;
unsigned short _LINES;
unsigned short _COLOURS;
unsigned int *_COLOUR_PAIRS; /* basically this is an array, but as its
			       length is determined only when initscr
			       is run, I can only think to make it a
			       pointer and malloc the array into being
			       ... */

struct cursor_pos {
	unsigned int y, x;
};

WINDOW _stdscr = {
	.attrs = A_DEFAULT,
	.ori_y = 0,
	.ori_x = 0,
	.curs_y = 0,
	.curs_x = 0,
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
static void _wputch ( WINDOW *win, chtype ch, int wrap ) {
	/* make sure we set the screen cursor to the right position
	   first! */
	win->scr->movetoyx( win->scr, win->ori_y + win->curs_y,
				      win->ori_x + win->curs_x );
	win->scr->putc(win->scr, ch);
	if ( ++(win->curs_x) > win->width ) {
		if ( wrap == WRAP ) {
			win->curs_x = 0;
			/* specification says we should really scroll,
			   but we have no buffer to scroll with, so we
			   can only overwrite back at the beginning of
			   the window */
			win->curs_y += ( ( win->curs_y - win->height ) == 0 ?
					 -(win->curs_y) : 1 );
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
static void _wputchstr ( WINDOW *win, const chtype *chstr, int wrap, int n ) {
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
static void _wputstr ( WINDOW *win, const char *str, int wrap, int n ) {
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
	printf("\a");
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
	int corner = '+' | win->attrs; /* default corner character */
	return wborder( win, verch, verch, horch, horch,
			corner, corner, corner, corner );
}

/**
 * Indicates whether the underlying terminal device is capable of
 * having colours redefined
 *
 * @ret bool	returns boolean
 */
bool can_change_colour ( void ) {
	return (bool)TRUE;
}

/**
 * Identify the RGB components of a given colour value
 *
 * @v colour	colour value
 * @v *red	address to store red component
 * @v *green	address to store green component
 * @v *blue	address to store blue component
 * @ret rc	return status code
 */
int colour_content ( short colour, short *red, short *green, short *blue ) {
	/* we do not have a particularly large range of colours (3
	   primary, 3 secondary and black), so let's just put in a
	   basic switch... */
	switch(colour) {
	case COLOUR_BLACK:
		*red = 0; *green = 0; *blue = 0;
		break;
	case COLOUR_BLUE:
		*red = 0; *green = 0; *blue = 1000;
		break;
	case COLOUR_GREEN:
		*red = 0; *green = 1000; *blue = 0;
		break;
	case COLOUR_CYAN:
		*red = 0; *green = 1000; *blue = 1000;
		break;
	case COLOUR_RED:
		*red = 1000; *green = 0; *blue = 0;
		break;
	case COLOUR_MAGENTA:
		*red = 1000; *green = 0; *blue = 1000;
		break;
	case COLOUR_YELLOW:
		*red = 1000; *green = 1000; *blue = 0;
		break;
	}
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
 * Get the background rendition attributes for a window
 *
 * @v *win	subject window
 * @ret ch	chtype rendition representation
 */
inline chtype getbkgd ( WINDOW *win ) {
	return win->attrs;
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
	_wputch( win, ch, WRAP );
	return OK;
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
	_wputstr( win, str, WRAP, n );
	return OK;
}

/**
 * Turn off attributes in a window
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
 * Turn on attributes in a window
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
 * Set attributes in a window
 *
 * @v win	subject window
 * @v attrs	attributes to enable
 * @ret rc	return status code
 */
int wattrset ( WINDOW *win, int attrs ) {
	win->attrs = ( attrs | ( win->attrs & A_COLOR ) );
	return OK;
}

/**
 * Get attributes and colour pair information
 *
 * @v *win	window to obtain information from
 * @v *attrs	address in which to store attributes
 * @v *pair	address in which to store colour pair
 * @v *opts	undefined (for future implementation)
 * @ret rc	return status cude
 */
int wattr_get ( WINDOW *win, attr_t *attrs, short *pair, void *opts ) {
	*attrs = win->attrs & A_ATTRIBUTES;
	*pair = (short)(( win->attrs & A_COLOR ) >> CPAIR_SHIFT);
	return OK;
}

/**
 * Turn off attributes in a window
 *
 * @v *win	subject window
 * @v attrs	attributes to toggle
 * @v *opts	undefined (for future implementation)
 * @ret rc	return status code
 */
int wattr_off ( WINDOW *win, attr_t attrs, void *opts ) {
	wattroff( win, attrs );
	return OK;
}

/**
 * Turn on attributes in a window
 *
 * @v *win	subject window
 * @v attrs	attributes to toggle
 * @v *opts	undefined (for future implementation)
 * @ret rc	return status code
 */
int wattr_on ( WINDOW *win, attr_t attrs, void *opts ) {
	wattron( win, attrs );
	return OK;
}

/**
 * Set attributes and colour pair information in a window
 *
 * @v *win	subject window
 * @v attrs	attributes to set
 * @v cpair	colour pair to set
 * @v *opts	undefined (for future implementation)
 * @ret rc	return status code
 */
int wattr_set ( WINDOW *win, attr_t attrs, short cpair, void *opts ) {
	wattrset( win, attrs | ( ( (unsigned short)cpair ) << CPAIR_SHIFT ) );
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

	wmove(win,0,0);

	_wputch(win,tl,WRAP);
	while ( win->width - win->curs_x ) {
		_wputch(win,ts,WRAP);
	}
	_wputch(win,tr,WRAP);

	while ( ( win->height - 1 ) - win->curs_y ) {
		_wputch(win,ls,WRAP);
		wmove(win,win->curs_y,win->width-1);
		_wputch(win,rs,WRAP);
	}

	_wputch(win,bl,WRAP);
	while ( win->width - win->curs_x ) {
		_wputch(win,bs,WRAP);
	}
	_wputch(win,br,NOWRAP); /* do not wrap last char to leave
				   cursor in last position */

	return OK;
}

/**
 * Clear a window to the bottom
 *
 * @v *win	subject window
 * @ret rc	return status code
 */
int wclrtobot ( WINDOW *win ) {
	struct cursor_pos pos;

	_store_curs_pos( win, &pos );
	while ( win->curs_y + win->curs_x ) {
		_wputch( win, (unsigned)' ', WRAP );
	}
	_restore_curs_pos( win, &pos );

	return OK;
}

/**
 * Clear a window to the end of the current line
 *
 * @v *win	subject window
 * @ret rc	return status code
 */
int wclrtoeol ( WINDOW *win ) {
	struct cursor_pos pos;

	_store_curs_pos( win, &pos );
	while ( ( win->curs_y - pos.y ) == 0 ) {
		_wputch( win, (unsigned)' ', WRAP );
	}
	_restore_curs_pos( win, &pos );

	return OK;
}

/**
 * Set colour pair for a window
 *
 * @v *win			subject window
 * @v colour_pair_number	colour pair integer
 * @v *opts			undefined (for future implementation)
 * @ret rc			return status code
 */
int wcolour_set ( WINDOW *win, short colour_pair_number, void *opts ) {
	if ( ( unsigned short )colour_pair_number > COLORS )
		return ERR;

	win->attrs = ( (unsigned short)colour_pair_number << CPAIR_SHIFT ) |
		( win->attrs & A_ATTRIBUTES );
	return OK;
}

