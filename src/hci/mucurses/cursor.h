#ifndef CURSOR_H
#define CURSOR_H

/** @file
 *
 * MuCurses cursor implementation specific header file
 *
 */

struct cursor_pos {
	unsigned int y, x;
};

void _restore_curs_pos ( WINDOW *win, struct cursor_pos *pos );
void _store_curs_pos ( WINDOW *win, struct cursor_pos *pos );

#endif /* CURSOR_H */
