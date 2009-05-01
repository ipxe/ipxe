/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <console.h>
#include <gpxe/keys.h>
#include <gpxe/editstring.h>
#include <readline/readline.h>

/** @file
 *
 * Minimal readline
 *
 */

#define READLINE_MAX 256

static void sync_console ( struct edit_string *string ) __nonnull;

/**
 * Synchronise console with edited string
 *
 * @v string		Editable string
 */
static void sync_console ( struct edit_string *string ) {
	unsigned int mod_start = string->mod_start;
	unsigned int mod_end = string->mod_end;
	unsigned int cursor = string->last_cursor;
	size_t len = strlen ( string->buf );

	/* Expand region back to old cursor position if applicable */
	if ( mod_start > string->last_cursor )
		mod_start = string->last_cursor;

	/* Expand region forward to new cursor position if applicable */
	if ( mod_end < string->cursor )
		mod_end = string->cursor;

	/* Backspace to start of region */
	while ( cursor > mod_start ) {
		putchar ( '\b' );
		cursor--;
	}

	/* Print modified region */
	while ( cursor < mod_end ) {
		putchar ( ( cursor >= len ) ? ' ' : string->buf[cursor] );
		cursor++;
	}

	/* Backspace to new cursor position */
	while ( cursor > string->cursor ) {
		putchar ( '\b' );
		cursor--;
	}
}

/**
 * Read line from console
 *
 * @v prompt		Prompt string
 * @ret line		Line read from console (excluding terminating newline)
 *
 * The returned line is allocated with malloc(); the caller must
 * eventually call free() to release the storage.
 */
char * readline ( const char *prompt ) {
	char buf[READLINE_MAX];
	struct edit_string string;
	int key;
	char *line;

	if ( prompt )
		printf ( "%s", prompt );

	memset ( &string, 0, sizeof ( string ) );
	string.buf = buf;
	string.len = sizeof ( buf );
	buf[0] = '\0';

	while ( 1 ) {
		key = edit_string ( &string, getkey() );
		sync_console ( &string );
		switch ( key ) {
		case CR:
		case LF:
			putchar ( '\n' );
			line = strdup ( buf );
			if ( ! line )
				printf ( "Out of memory\n" );
			return line;
		case CTRL_C:
			putchar ( '\n' );
			return NULL;
		default:
			/* Do nothing */
			break;
		}
	}
}
