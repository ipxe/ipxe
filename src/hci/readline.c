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

#include <string.h>
#include <malloc.h>
#include <console.h>
#include <gpxe/editstring.h>
#include <readline/readline.h>

/** @file
 *
 * Minimal readline
 *
 */

#define READLINE_MAX 256

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
	struct edit_string string = {
		.buf = buf,
		.len = sizeof ( buf ),
		.cursor = 0,
	};
	int key;

	if ( prompt )
		printf ( "%s", prompt );

	buf[0] = '\0';
	while ( 1 ) {
		key = edit_string ( &string, getchar() );
		switch ( key ) {
		case 0x0d: /* Carriage return */
		case 0x0a: /* Line feed */
			return ( strdup ( buf ) );
		case 0x03: /* Ctrl-C */
			return NULL;
		default:
			/* Do nothing */
			break;
		}
	}
}
