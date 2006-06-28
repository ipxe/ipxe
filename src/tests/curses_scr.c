#include "../include/curses.h"
#include <termios.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

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
	//printf("%c[=%dh",ESC,MODE);
	LINES = 25; COLS = 80;
}

void _exit_screen( struct _curses_screen *scr __unused ) {
	printf("%c[1;1H",ESC);
	printf("%c[2J",ESC);
	tcsetattr(fileno(stdin),TCSANOW,&original);
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
	//printf("char is \"%d\"", c );
	putchar( c );
	fflush(stdout); // There must be a better way to do this...
}

int _getc( struct _curses_screen *scr __unused ) {
	int c;
	char buffer[16];
	char *ptr;
	c = getchar();
	if ( c == '\n' )
		return KEY_ENTER;
	/*
	  WE NEED TO PROCESS ANSI SEQUENCES TO PASS BACK KEY_* VALUES
	if ( c == ESC ) {
		ptr = buffer;
		while ( scr->peek( scr ) == TRUE ) {
			*(ptr++) = getchar();
		}

		// ANSI sequences
		if ( strcmp ( buffer, "[D" ) == 0 )
			return KEY_LEFT;
	}
	*/

	return c;
}

bool _peek( struct _curses_screen *scr __unused ) {
	int c;
	if ( ( c = getchar() ) != EOF ) {
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
