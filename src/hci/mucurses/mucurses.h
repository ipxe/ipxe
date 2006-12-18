#ifndef _MUCURSES_H
#define _MUCURSES_H

/** @file
 *
 * MuCurses core implementation specific header file
 *
 */

#define WRAP 0
#define NOWRAP 1

void _wputch ( WINDOW *win, chtype ch, int wrap );
void _wputchstr ( WINDOW *win, const chtype *chstr, int wrap, int n );
void _wputstr ( WINDOW *win, const char *str, int wrap, int n );
void _wcursback ( WINDOW *win );

#endif /* _MUCURSES_H */
