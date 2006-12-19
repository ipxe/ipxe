#include <curses.h>
#include <console.h>

unsigned short _COLS = 80;
unsigned short _LINES = 25;

static void ansiscr_init ( struct _curses_screen *scr __unused ) {
}

static void ansiscr_exit ( struct _curses_screen *scr __unused ) {
}

static void ansiscr_movetoyx ( struct _curses_screen *scr __unused,
			       unsigned int y, unsigned int x ) {
	/* ANSI escape sequence to update cursor position */
	printf ( "\033[%d;%dH", ( y + 1 ), ( x + 1 ) );
}

static void ansiscr_putc ( struct _curses_screen *scr, chtype c ) {
	unsigned int character = ( c & A_CHARTEXT );
	attr_t attrs = ( c & ( A_ATTRIBUTES | A_COLOR ) );
	int bold = ( attrs & A_BOLD );
	attr_t cpair = PAIR_NUMBER ( attrs );
	short fcol;
	short bcol;

	if ( attrs != scr->attrs ) {
		scr->attrs = attrs;
		pair_content ( cpair, &fcol, &bcol );
		/* ANSI escape sequence to update character attributes */
		printf ( "\033[0;%d;3%d;4%dm", ( bold ? 1 : 22 ), fcol, bcol );
	}
	putchar ( character );
}

static int ansiscr_getc ( struct _curses_screen *scr __unused ) {
	return getchar();
}

static bool ansiscr_peek ( struct _curses_screen *scr __unused ) {
	return iskey();
}

SCREEN _ansi_screen = {
	.init		= ansiscr_init,
	.exit		= ansiscr_exit,
	.movetoyx	= ansiscr_movetoyx,
	.putc		= ansiscr_putc,
	.getc		= ansiscr_getc,
	.peek		= ansiscr_peek,
};
