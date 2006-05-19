#ifndef CURSES_H
#define CURSES_H

#include <stdint.h>
#include <stdarg.h>

/** @file
 *
 * MuCurses header file
 *
 */

#undef  ERR
#define ERR	(1)

#undef  FALSE
#define FALSE	(0)

#undef  OK
#define OK	(0)

#undef  TRUE
#define TRUE	(1)

typedef uint32_t bool;
typedef uint32_t chtype;
typedef chtype attr_t;

/** Curses SCREEN object */
typedef struct _curses_screen {
	/**
	 * Move cursor to position specified by x,y coords
	 *
	 * @v scr	screen on which to operate
	 * @v y		Y position
	 * @v x		X position
	 */
	void ( * movetoyx ) ( struct _curses_screen *scr,
			      unsigned int y, unsigned int x );
	/**
	 * Write character to current cursor position
	 *
	 * @v scr	screen on which to operate
	 * @v c		character to be written
	 */
	void ( * putc ) ( struct _curses_screen *scr, chtype c );
	/**
	 * Read a character
	 *
	 * @v scr	screen on which to operate
	 * @ret c	character
	 */
	int ( * getc ) ( struct _curses_screen *scr );
} SCREEN;

/** Curses Window struct */
typedef struct _curses_window {
	/** screen with which window associates */
	SCREEN *scr;
	/** window attributes */
	attr_t attrs;
	/** window origin coordinates */
	unsigned int ori_x, ori_y;
	/** window cursor position */
	unsigned int curs_x, curs_y;
	/** window dimensions */
	unsigned int width, height;
} WINDOW;

extern WINDOW _stdscr;
extern SCREEN _curscr;
extern unsigned short _COLS;
extern unsigned short _LINES;
extern unsigned short _COLOURS;
extern unsigned int *_COLOUR_PAIRS;

#define stdscr ( &_stdscr )
#define curscr ( &_curscr )
#define COLS _COLS
#define LINES _LINES
#define COLORS _COLOURS
#define COLOR_PAIRS COLOUR_PAIRS

#define MUCURSES_BITS( mask, shift ) (( mask ) << (shift))
#define CPAIR_SHIFT	8
#define ATTRS_SHIFT	16

#define A_DEFAULT	( 1UL - 1UL )
#define A_ALTCHARSET	MUCURSES_BITS( 1UL, ATTRS_SHIFT + 0 )
#define A_BLINK		MUCURSES_BITS( 1UL, ATTRS_SHIFT + 1 )
#define A_BOLD		MUCURSES_BITS( 1UL, ATTRS_SHIFT + 2 )
#define A_DIM		MUCURSES_BITS( 1UL, ATTRS_SHIFT + 3 )
#define A_INVIS		MUCURSES_BITS( 1UL, ATTRS_SHIFT + 4 )
#define A_PROTECT	MUCURSES_BITS( 1UL, ATTRS_SHIFT + 5 )
#define A_REVERSE	MUCURSES_BITS( 1UL, ATTRS_SHIFT + 6 )
#define A_STANDOUT	MUCURSES_BITS( 1UL, ATTRS_SHIFT + 7 )
#define A_UNDERLINE	MUCURSES_BITS( 1UL, ATTRS_SHIFT + 8 )

#define WA_ALTCHARSET	A_ALTCHARSET
#define WA_BLINK	A_BLINK
#define WA_BOLD		A_BOLD
#define WA_DIM		A_DIM
#define WA_INVIS	A_INVIS
#define WA_PROTECT	A_PROTECT
#define WA_REVERSE	A_REVERSE
#define WA_STANDOUT	A_STANDOUT
#define WA_UNDERLINE	A_UNDERLINE
#define WA_HORIZONTAL	MUCURSES_BITS( 1UL, ATTRS_SHIFT + 9 )
#define WA_VERTICAL	MUCURSES_BITS( 1UL, ATTRS_SHIFT + 10 )
#define WA_LEFT		MUCURSES_BITS( 1UL, ATTRS_SHIFT + 11 )
#define WA_RIGHT	MUCURSES_BITS( 1UL, ATTRS_SHIFT + 12 )
#define WA_LOW		MUCURSES_BITS( 1UL, ATTRS_SHIFT + 13 )
#define WA_TOP		MUCURSES_BITS( 1UL, ATTRS_SHIFT + 14 )

#define A_ATTRIBUTES	( MUCURSES_BITS( 1UL, ATTRS_SHIFT ) - 1UL )
#define A_CHARTEXT	( MUCURSES_BITS( 1UL, 0 ) - 1UL )
#define A_COLOR		MUCURSES_BITS( ( 1UL << 8 ) - 1UL, CPAIR_SHIFT )

#define ACS_ULCORNER	'+'
#define ACS_LLCORNER	'+'
#define ACS_URCORNER	'+'
#define ACS_LRCORNER	'+'
#define ACS_RTEE	'+'
#define ACS_LTEE	'+'
#define ACS_BTEE	'+'
#define ACS_TTEE	'+'
#define ACS_HLINE	'-'
#define ACS_VLINE	'|'
#define ACS_PLUS	'+'
#define ACS_S1		'-'
#define ACS_S9		'_'
#define ACS_DIAMOND	'+'
#define ACS_CKBOARD	':'
#define ACS_DEGREE	'\''
#define ACS_PLMINUS	'#'
#define ACS_BULLET	'o'
#define ACS_LARROW	'<'
#define ACS_RARROW	'>'
#define ACS_DARROW	'v'
#define ACS_UARROW	'^'
#define ACS_BOARD	'#'
#define ACS_LANTERN	'#'
#define ACS_BLOCK	'#'

#define COLOUR_BLACK	0
#define COLOUR_BLUE	1
#define COLOUR_GREEN	2
#define COLOUR_CYAN	3
#define COLOUR_RED	4
#define COLOUR_MAGENTA	5
#define COLOUR_YELLOW	6
#define COLOUR_WHITE	7

#define COLOR_BLACK	COLOUR_BLACK
#define COLOR_BLUE	COLOUR_BLUE
#define COLOR_GREEN	COLOUR_GREEN
#define COLOR_CYAN	COLOUR_CYAN
#define COLOR_RED	COLOUR_RED
#define COLOR_MAGENTA	COLOUR_MAGENTA
#define COLOR_YELLOW	COLOUR_YELLOW
#define COLOR_WHITE	COLOUR_WHITE

/*
 * KEY code constants
 */
#define KEY_BREAK	0401		/**< Break key */
#define KEY_DOWN	0402		/**< down-arrow key */
#define KEY_UP		0403		/**< up-arrow key */
#define KEY_LEFT	0404		/**< left-arrow key */
#define KEY_RIGHT	0405		/**< right-arrow key */
#define KEY_HOME	0406		/**< home key */
#define KEY_BACKSPACE	0407		/**< backspace key */
#define KEY_F0		0410		/**< Function keys.  Space for 64 */
#define KEY_F(n)	(KEY_F0+(n))	/**< Value of function key n */
#define KEY_DL		0510		/**< delete-line key */
#define KEY_IL		0511		/**< insert-line key */
#define KEY_DC		0512		/**< delete-character key */
#define KEY_IC		0513		/**< insert-character key */
#define KEY_EIC		0514		/**< sent by rmir or smir in insert mode */
#define KEY_CLEAR	0515		/**< clear-screen or erase key */
#define KEY_EOS		0516		/**< clear-to-end-of-screen key */
#define KEY_EOL		0517		/**< clear-to-end-of-line key */
#define KEY_SF		0520		/**< scroll-forward key */
#define KEY_SR		0521		/**< scroll-backward key */
#define KEY_NPAGE	0522		/**< next-page key */
#define KEY_PPAGE	0523		/**< previous-page key */
#define KEY_STAB	0524		/**< set-tab key */
#define KEY_CTAB	0525		/**< clear-tab key */
#define KEY_CATAB	0526		/**< clear-all-tabs key */
#define KEY_ENTER	0527		/**< enter/send key */
#define KEY_PRINT	0532		/**< print key */
#define KEY_LL		0533		/**< lower-left key (home down) */
#define KEY_A1		0534		/**< upper left of keypad */
#define KEY_A3		0535		/**< upper right of keypad */
#define KEY_B2		0536		/**< center of keypad */
#define KEY_C1		0537		/**< lower left of keypad */
#define KEY_C3		0540		/**< lower right of keypad */
#define KEY_BTAB	0541		/**< back-tab key */
#define KEY_BEG		0542		/**< begin key */
#define KEY_CANCEL	0543		/**< cancel key */
#define KEY_CLOSE	0544		/**< close key */
#define KEY_COMMAND	0545		/**< command key */
#define KEY_COPY	0546		/**< copy key */
#define KEY_CREATE	0547		/**< create key */
#define KEY_END		0550		/**< end key */
#define KEY_EXIT	0551		/**< exit key */
#define KEY_FIND	0552		/**< find key */
#define KEY_HELP	0553		/**< help key */
#define KEY_MARK	0554		/**< mark key */
#define KEY_MESSAGE	0555		/**< message key */
#define KEY_MOVE	0556		/**< move key */
#define KEY_NEXT	0557		/**< next key */
#define KEY_OPEN	0560		/**< open key */
#define KEY_OPTIONS	0561		/**< options key */
#define KEY_PREVIOUS	0562		/**< previous key */
#define KEY_REDO	0563		/**< redo key */
#define KEY_REFERENCE	0564		/**< reference key */
#define KEY_REFRESH	0565		/**< refresh key */
#define KEY_REPLACE	0566		/**< replace key */
#define KEY_RESTART	0567		/**< restart key */
#define KEY_RESUME	0570		/**< resume key */
#define KEY_SAVE	0571		/**< save key */
#define KEY_SBEG	0572		/**< shifted begin key */
#define KEY_SCANCEL	0573		/**< shifted cancel key */
#define KEY_SCOMMAND	0574		/**< shifted command key */
#define KEY_SCOPY	0575		/**< shifted copy key */
#define KEY_SCREATE	0576		/**< shifted create key */
#define KEY_SDC		0577		/**< shifted delete-character key */
#define KEY_SDL		0600		/**< shifted delete-line key */
#define KEY_SELECT	0601		/**< select key */
#define KEY_SEND	0602		/**< shifted end key */
#define KEY_SEOL	0603		/**< shifted clear-to-end-of-line key */
#define KEY_SEXIT	0604		/**< shifted exit key */
#define KEY_SFIND	0605		/**< shifted find key */
#define KEY_SHELP	0606		/**< shifted help key */
#define KEY_SHOME	0607		/**< shifted home key */
#define KEY_SIC		0610		/**< shifted insert-character key */
#define KEY_SLEFT	0611		/**< shifted left-arrow key */
#define KEY_SMESSAGE	0612		/**< shifted message key */
#define KEY_SMOVE	0613		/**< shifted move key */
#define KEY_SNEXT	0614		/**< shifted next key */
#define KEY_SOPTIONS	0615		/**< shifted options key */
#define KEY_SPREVIOUS	0616		/**< shifted previous key */
#define KEY_SPRINT	0617		/**< shifted print key */
#define KEY_SREDO	0620		/**< shifted redo key */
#define KEY_SREPLACE	0621		/**< shifted replace key */
#define KEY_SRIGHT	0622		/**< shifted right-arrow key */
#define KEY_SRSUME	0623		/**< shifted resume key */
#define KEY_SSAVE	0624		/**< shifted save key */
#define KEY_SSUSPEND	0625		/**< shifted suspend key */
#define KEY_SUNDO	0626		/**< shifted undo key */
#define KEY_SUSPEND	0627		/**< suspend key */
#define KEY_UNDO	0630		/**< undo key */
#define KEY_RESIZE	0632		/**< Terminal resize event */
#define KEY_EVENT	0633		/**< We were interrupted by an event */

#define KEY_MAX		0777		/* Maximum key value is 0633 */

/*extern int addch ( const chtype * );*/
/*extern int addchnstr ( const chtype *, int );*/
/*extern int addchstr ( const chtype * );*/
/*extern int addnstr ( const char *, int );*/
/*extern int addstr ( const char * );*/
/*extern int attroff ( int );*/
/*extern int attron ( int );*/
/*extern int attrset ( int );*/
extern int attr_get ( attr_t *, short *, void * );
extern int attr_off ( attr_t, void * );
extern int attr_on ( attr_t, void * );
extern int attr_set ( attr_t, short, void * );
extern int baudrate ( void );
extern int beep ( void );
/*extern void bkgdset ( chtype );*/
/*extern int border ( chtype, chtype, chtype, chtype, chtype, chtype, chtype,
  chtype );*/
extern int box ( WINDOW *, chtype, chtype );
extern bool can_change_colour ( void );
#define can_change_color() can_change_colour()
extern int cbreak ( void ); 
/*extern int clrtobot ( void );*/
/*extern int clrtoeol ( void );*/
extern int colour_content ( short, short *, short *, short * );
#define color_content( col, r, g, b ) colour_content( (col), (r), (g), (b) )
/*extern int colour_set ( short, void * );*/
/*#define color_set( cpno, opts ) colour_set( (cpno), (opts) )*/
extern int copywin ( const WINDOW *, WINDOW *, int, int, int, 
		     int, int, int, int );
extern int curs_set ( int );
extern int def_prog_mode ( void );
extern int def_shell_mode ( void );
extern int delay_output ( int );
extern int delch ( void );
extern int deleteln ( void );
extern void delscreen ( SCREEN * ); 
extern int delwin ( WINDOW * );
extern WINDOW *derwin ( WINDOW *, int, int, int, int );
extern int doupdate ( void );
extern WINDOW *dupwin ( WINDOW * );
extern int echo ( void );
extern int echochar ( const chtype );
extern int endwin ( void );
extern char erasechar ( void );
extern int erase ( void );
extern void filter ( void );
extern int flash ( void );
extern int flushinp ( void );
extern chtype getbkgd ( WINDOW * );
extern int getch ( void );
extern int getnstr ( char *, int );
extern int getstr ( char * );
extern int halfdelay ( int );
extern bool has_colors ( void );
extern bool has_ic ( void );
extern bool has_il ( void );
extern int hline ( chtype, int );
extern void idcok ( WINDOW *, bool );
extern int idlok ( WINDOW *, bool );
extern void immedok ( WINDOW *, bool );
extern chtype inch ( void );
extern int inchnstr ( chtype *, int );
extern int inchstr ( chtype * );
extern WINDOW *initscr ( void );
extern int init_color ( short, short, short, short );
extern int init_pair ( short, short, short );
extern int innstr ( char *, int );
extern int insch ( chtype );
extern int insdelln ( int );
extern int insertln ( void );
extern int insnstr ( const char *, int );
extern int insstr ( const char * );
extern int instr ( char * );
extern int intrflush ( WINDOW *, bool );
extern bool isendwin ( void );
extern bool is_linetouched ( WINDOW *, int );
extern bool is_wintouched ( WINDOW * );
extern char *keyname ( int );
extern int keypad ( WINDOW *, bool );
extern char killchar ( void );
extern int leaveok ( WINDOW *, bool );
extern char *longname ( void );
extern int meta ( WINDOW *, bool );
/*extern int move ( int, int );*/
/*extern int mvaddch ( int, int, const chtype );*/
/*extern int mvaddchnstr ( int, int, const chtype *, int );*/
/*extern int mvaddchstr ( int, int, const chtype * );*/
/*extern int mvaddnstr ( int, int, const char *, int );*/
/*extern int mvaddstr ( int, int, const char * );*/
extern int mvcur ( int, int, int, int );
extern int mvdelch ( int, int );
extern int mvderwin ( WINDOW *, int, int );
extern int mvgetch ( int, int );
extern int mvgetnstr ( int, int, char *, int );
extern int mvgetstr ( int, int, char * );
extern int mvhline ( int, int, chtype, int );
extern chtype mvinch ( int, int );
extern int mvinchnstr ( int, int, chtype *, int );
extern int mvinchstr ( int, int, chtype * );
extern int mvinnstr ( int, int, char *, int );
extern int mvinsch ( int, int, chtype );
extern int mvinsnstr ( int, int, const char *, int );
extern int mvinsstr ( int, int, const char * );
extern int mvinstr ( int, int, char * );
extern int mvprintw ( int, int, char *,  ... );
extern int mvscanw ( int, int, char *, ... );
extern int mvvline ( int, int, chtype, int );
/*extern int mvwaddch ( WINDOW *, int, int, const chtype );*/
/*extern int mvwaddchnstr ( WINDOW *, int, int, const chtype *, int );*/
/*extern int mvwaddchstr ( WINDOW *, int, int, const chtype * );*/
/*extern int mvwaddnstr ( WINDOW *, int, int, const char *, int );*/
/*extern int mvwaddstr ( WINDOW *, int, int, const char * );*/
extern int mvwdelch ( WINDOW *, int, int );
extern int mvwgetch ( WINDOW *, int, int );
extern int mvwgetnstr ( WINDOW *, int, int, char *, int );
extern int mvwgetstr ( WINDOW *, int, int, char * );
extern int mvwhline ( WINDOW *, int, int, chtype, int );
extern int mvwin ( WINDOW *, int, int );
extern chtype mvwinch ( WINDOW *, int, int );
extern int mvwinchnstr ( WINDOW *, int, int, chtype *, int );
extern int mvwinchstr ( WINDOW *, int, int, chtype * );
extern int mvwinnstr ( WINDOW *, int, int, char *, int );
extern int mvwinsch ( WINDOW *, int, int, chtype );
extern int mvwinsnstr ( WINDOW *, int, int, const char *, int );
extern int mvwinsstr ( WINDOW *, int, int, const char * );
extern int mvwinstr ( WINDOW *, int, int, char * );
extern int mvwprintw ( WINDOW *, int, int, char *, ... );
extern int mvwscanw ( WINDOW *, int, int, char *, ... );
extern int mvwvline ( WINDOW *, int, int, chtype, int );
extern int napms ( int );
extern WINDOW *newpad ( int, int );
extern WINDOW *newwin ( int, int, int, int );
extern int nl ( void );
extern int nocbreak ( void );
extern int nodelay ( WINDOW *, bool );
extern int noecho ( void );
extern int nonl ( void );
extern void noqiflush ( void );
extern int noraw ( void );
extern int notimeout ( WINDOW *, bool );
extern int overlay ( const WINDOW *, WINDOW * );
extern int overwrite ( const WINDOW *, WINDOW * );
extern int pair_content ( short, short *, short * );
extern int PAIR_NUMBER ( int );
extern int pechochar ( WINDOW *, chtype );
extern int pnoutrefresh ( WINDOW *, int, int, int, int, int, int );
extern int prefresh ( WINDOW *, int, int, int, int, int, int );
extern int printw ( char *, ... );
extern int putp ( const char * );
extern void qiflush ( void );
extern int raw ( void );
extern int redrawwin ( WINDOW * );
extern int refresh ( void );
extern int reset_prog_mode ( void );
extern int reset_shell_mode ( void );
extern int resetty ( void );
extern int ripoffline ( int, int  ( *) ( WINDOW *, int) );
extern int savetty ( void );
extern int scanw ( char *, ... );
extern int scr_dump ( const char * );
extern int scr_init ( const char * );
extern int scrl ( int );
extern int scroll ( WINDOW * );
extern int scrollok ( WINDOW *, bool );
extern int scr_restore ( const char * );
extern int scr_set ( const char * );
extern int setscrreg ( int, int );
extern SCREEN *set_term ( SCREEN * );
extern int setupterm ( char *, int, int * );
extern int slk_attr_off ( const attr_t, void * );
extern int slk_attroff ( const chtype );
extern int slk_attr_on ( const attr_t, void * );
extern int slk_attron ( const chtype );
extern int slk_attr_set ( const attr_t, short, void * );
extern int slk_attrset ( const chtype );
extern int slk_clear ( void );
extern int slk_color ( short );
extern int slk_init ( int );
extern char *slk_label ( int );
extern int slk_noutrefresh ( void );
extern int slk_refresh ( void );
extern int slk_restore ( void );
extern int slk_set ( int, const char *, int );
extern int slk_touch ( void );
extern int standend ( void );
extern int standout ( void );
extern int start_color ( void );
extern WINDOW *subpad ( WINDOW *, int, int, int, int );
extern WINDOW *subwin ( WINDOW *, int, int, int, int );
extern int syncok ( WINDOW *, bool );
extern chtype termattrs ( void );
extern attr_t term_attrs ( void );
extern char *termname ( void );
extern int tigetflag ( char * );
extern int tigetnum ( char * );
extern char *tigetstr ( char * );
extern void timeout ( int );
extern int touchline ( WINDOW *, int, int );
extern int touchwin ( WINDOW * );
extern char *tparm ( char *, long, long, long, long, long, long, long, long,
		   long );
extern int typeahead ( int );
extern int ungetch ( int );
extern int untouchwin ( WINDOW * );
extern void use_env ( bool );
extern int vid_attr ( attr_t, short, void * );
extern int vidattr ( chtype );
extern int vid_puts ( attr_t, short, void *, int  ( *) ( int) );
extern int vidputs ( chtype, int  ( *) ( int) );
extern int vline ( chtype, int );
extern int vwprintw ( WINDOW *, char *, va_list * );
extern int vw_printw ( WINDOW *, char *, va_list * );
extern int vwscanw ( WINDOW *, char *, va_list * );
extern int vw_scanw ( WINDOW *, char *, va_list * );
extern int waddch ( WINDOW *, const chtype );
extern int waddchnstr ( WINDOW *, const chtype *, int );
/*extern int waddchstr ( WINDOW *, const chtype * );*/
extern int waddnstr ( WINDOW *, const char *, int );
/*extern int waddstr ( WINDOW *, const char * );*/
extern int wattroff ( WINDOW *, int );
extern int wattron ( WINDOW *, int );
extern int wattrset ( WINDOW *, int );
extern int wattr_get ( WINDOW *, attr_t *, short *, void * );
extern int wattr_off ( WINDOW *, attr_t, void * );
extern int wattr_on ( WINDOW *, attr_t, void * );
extern int wattr_set ( WINDOW *, attr_t, short, void * );
/*extern void wbkgdset ( WINDOW *, chtype );*/
extern int wborder ( WINDOW *, chtype, chtype, chtype, chtype, chtype, chtype,
		   chtype, chtype );
extern int wclrtobot ( WINDOW * );
extern int wclrtoeol ( WINDOW * );
extern void wcursyncup ( WINDOW * );
/*extern int wcolor_set ( WINDOW *, short, void * );*/
#define wcolor_set(w,s,v) wcolour_set((w),(s),(v))
extern int wdelch ( WINDOW * );
extern int wdeleteln ( WINDOW * );
extern int wechochar ( WINDOW *, const chtype );
extern int werase ( WINDOW * );
extern int wgetch ( WINDOW * );
extern int wgetnstr ( WINDOW *, char *, int );
extern int wgetstr ( WINDOW *, char * );
extern int whline ( WINDOW *, chtype, int );
extern chtype winch ( WINDOW * );
extern int winchnstr ( WINDOW *, chtype *, int );
extern int winchstr ( WINDOW *, chtype * );
extern int winnstr ( WINDOW *, char *, int );
extern int winsch ( WINDOW *, chtype );
extern int winsdelln ( WINDOW *, int );
extern int winsertln ( WINDOW * );
extern int winsnstr ( WINDOW *, const char *, int );
extern int winsstr ( WINDOW *, const char * );
extern int winstr ( WINDOW *, char * );
extern int wmove ( WINDOW *, int, int );
extern int wnoutrefresh ( WINDOW * );
extern int wprintw ( WINDOW *, char *, ... );
extern int wredrawln ( WINDOW *, int, int );
extern int wrefresh ( WINDOW * );
extern int wscanw ( WINDOW *, char *, ... );
extern int wscrl ( WINDOW *, int );
extern int wsetscrreg ( WINDOW *, int, int );
extern int wstandend ( WINDOW * );
extern int wstandout ( WINDOW * );
extern void wsyncup ( WINDOW * );
extern void wsyncdown ( WINDOW * );
extern void wtimeout ( WINDOW *, int );
extern int wtouchln ( WINDOW *, int, int, int );
extern int wvline ( WINDOW *, chtype, int );

/*
 * There is frankly a ridiculous amount of redundancy within the
 * curses API - ncurses decided to get around this by using #define
 * macros, but I've decided to be type-safe and implement them all as
 * static inlines instead...
 */

static inline int addch ( const chtype ch ) {
	return waddch( stdscr, ch );
}

static inline int addchnstr ( const chtype *chstr, int n ) {
	return waddchnstr ( stdscr, chstr, n );
}

static inline int addchstr ( const chtype *chstr ) {
	return waddchnstr ( stdscr, chstr, -1 );
}

static inline int addnstr ( const char *str, int n ) {
	return waddnstr ( stdscr, str, n );
}

static inline int addstr ( const char *str ) {
	return waddnstr ( stdscr, str, -1 );
}

static inline int attroff ( int attrs ) {
	return wattroff ( stdscr, attrs );
}

static inline int attron ( int attrs ) {
	return wattron ( stdscr, attrs );
}

static inline int attrset ( int attrs ) {
	return wattrset ( stdscr, attrs );
}

static inline void bkgdset ( chtype ch ) {
	wattrset ( stdscr, ch );
}

static inline int border ( chtype ls, chtype rs, chtype ts, chtype bs,
			   chtype tl, chtype tr, chtype bl, chtype br ) {
	return wborder ( stdscr, ls, rs, ts, bs, tl, tr, bl, br );
}

static inline int clrtobot ( void ) {
	return wclrtobot( stdscr );
}

static inline int clrtoeol ( void ) {
	return wclrtoeol( stdscr );
}

static inline int move ( int y, int x ) {
	return wmove ( stdscr, y, x );
}

static inline int mvaddch ( int y, int x, const chtype ch ) {
	return ( wmove ( stdscr, y, x ) == ERR 
		 ? ERR : waddch( stdscr, ch ) );
}

static inline int mvaddchnstr ( int y, int x, const chtype *chstr, int n ) {
	return ( wmove ( stdscr, y, x ) == ERR
		 ? ERR : waddchnstr ( stdscr, chstr, n ) );
}

static inline int mvaddchstr ( int y, int x, const chtype *chstr ) {
	return ( wmove ( stdscr, y, x ) == ERR
		 ? ERR : waddchnstr ( stdscr, chstr, -1 ) );
}

static inline int mvaddnstr ( int y, int x, const char *str, int n ) {
	return ( wmove ( stdscr, y, x ) == ERR
		 ? ERR : waddnstr ( stdscr, str, n ) );
}

static inline int mvaddstr ( int y, int x, const char *str ) {
	return ( wmove ( stdscr, y, x ) == ERR
		 ? ERR : waddnstr ( stdscr, str, -1 ) );
}

static inline int mvwaddch ( WINDOW *win, int y, int x, const chtype ch ) {
	return ( wmove( win, y, x ) == ERR 
		 ? ERR : waddch ( win, ch ) );
}

static inline int mvwaddchnstr ( WINDOW *win, int y, int x, const chtype *chstr, int n ) {
	return ( wmove ( win, y, x ) == ERR 
		 ? ERR : waddchnstr ( win, chstr, n ) );
}

static inline int mvwaddchstr ( WINDOW *win, int y, int x, const chtype *chstr ) {
	return ( wmove ( win, y, x ) == ERR 
		 ? ERR : waddchnstr ( win, chstr, -1 ) );
}

static inline int mvwaddnstr ( WINDOW *win, int y, int x, const char *str, int n ) {
	return ( wmove ( win, y, x ) == ERR
		 ? ERR : waddnstr ( win, str, n ) );
}

static inline int mvwaddstr ( WINDOW *win, int y, int x, const char *str ) {
	return ( wmove ( win, y, x ) == ERR
		 ? ERR : waddnstr ( win, str, -1 ) );
}

static inline int waddchstr ( WINDOW *win, const chtype *chstr ) {
	return waddchnstr ( win, chstr, -1 );
}

static inline int waddstr ( WINDOW *win, const char *str ) {
	return waddnstr ( win, str, -1 );
}

static inline int wbkgdset ( WINDOW *win, chtype ch ) {
	return wattrset( win, ch );
}

#endif /* CURSES_H */
