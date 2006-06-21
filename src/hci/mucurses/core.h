#ifndef CORE_H
#define CORE_H

/** @file
 *
 * MuCurses core implementation specific header file
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

void _wputch ( WINDOW *win, chtype ch, int wrap );
void _wputchstr ( WINDOW *win, const chtype *chstr, int wrap, int n );
void _wputstr ( WINDOW *win, const char *str, int wrap, int n );
int wmove ( WINDOW *win, int y, int x );

#endif /* CURSES_H */
