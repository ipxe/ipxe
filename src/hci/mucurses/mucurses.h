#ifndef _MUCURSES_H
#define _MUCURSES_H

/** @file
 *
 * MuCurses core implementation specific header file
 *
 */

#define WRAP 0
#define NOWRAP 1

extern SCREEN _ansi_screen;

extern void _wputch ( WINDOW *win, chtype ch, int wrap );
extern void _wputchstr ( WINDOW *win, const chtype *chstr, int wrap, int n );
extern void _wputstr ( WINDOW *win, const char *str, int wrap, int n );
extern void _wcursback ( WINDOW *win );

#endif /* _MUCURSES_H */
