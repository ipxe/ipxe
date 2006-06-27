#include "/home/dan/cvs/extcvs/eb/src/include/curses.h"
#include <termios.h>
#include <stddef.h>
#include <stdio.h>

#define ESC 27
#define MODE 3

unsigned int _COLOUR_PAIRS = 4;
unsigned int _COLOURS = 8;
unsigned short _COLS = 80;
unsigned short _LINES = 25;

static struct termios original, runtime;

void _init_screen( struct _curses_screen *scr __unused ) {
	tcgetattr(fileno(stdin),&original);
	tcgetattr(fileno(stdin),&runtime);
	runtime.c_lflag &= ~(ICANON|ECHO);
	tcsetattr(fileno(stdin),TCSANOW,&runtime);
	printf("%c[=%dh",ESC,MODE);
	LINES = 25; COLS = 80;
}

void _exit_screen( struct _curses_screen *scr __unused ) {
	tcsetattr(fileno(stdin),TCSANOW,&original);
	printf("%c[0",ESC);
	printf("%c[u",ESC);
}

void _movetoyx( struct _curses_screen *scr __unused, unsigned int y, unsigned int x ) {
	printf( "%c[%d;%dH", ESC, y+1, x+1 );
}

void _putc( struct _curses_screen *scr __unused, chtype c ) {
	unsigned short pairno;
	pairno = (unsigned short)(( c & A_COLOUR ) >> CPAIR_SHIFT);
	
	// print rendition (colour and attrs)
	//printf( "%c[%d;%d",ESC, 
	//	cpairs[pairno][0], cpairs[pairno][1] );
	// print rendition (character)
	printf( "%c", (char)(c & A_CHARTEXT) );
}

int _getc( struct _curses_screen *scr __unused ) {
	return getc(stdin);
}

bool _peek( struct _curses_screen *scr __unused ) {
	int c;
	if ( ( c = getc(stdin) ) != EOF ) {
		ungetc( c, stdin );
		return TRUE;
	} else { return FALSE; }
}

SCREEN _curscr = {
	.init = _init_screen,
	.exit = _exit_screen,
	.movetoyx = _movetoyx,
	.putc = _putc,
	.getc = _getc,
	.peek = _peek,
};
