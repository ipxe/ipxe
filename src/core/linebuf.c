/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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

/**
 * @file
 *
 * Line buffering
 *
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <gpxe/linebuf.h>

/**
 * Line terminators
 *
 * These values are used in the @c skip_terminators bitmask.
 */
enum line_terminators {
	TERM_CR = 1,
	TERM_NL = 2,
	TERM_NUL = 4,
};

/**
 * Get terminator ID corresponding to character
 *
 * @v character		Character
 * @ret terminator_id	Terminator ID, or -1
 */
static int terminator_id ( unsigned int character ) {
	switch ( character ) {
	case '\r':
		return TERM_CR;
	case '\n':
		return TERM_NL;
	case '\0':
		return TERM_NUL;
	default:
		return -1;
	}
}

/**
 * Discard line buffer contents
 *
 * @v linebuf		Line buffer
 */
void empty_line_buffer ( struct line_buffer *linebuf ) {
	free ( linebuf->data );
	linebuf->data = NULL;
	linebuf->len = 0;
}

/**
 * Buffer up received data by lines
 *
 * @v linebuf			Line buffer
 * @v data			New data to add
 * @v len			Length of new data to add
 * @ret buffered		Amount of data consumed and added to the buffer
 * @ret <0			Out of memory
 * 
 * If line_buffer() does not consume the entirety of the new data
 * (i.e. if @c buffered is not equal to @c len), then an end of line
 * has been reached and the buffered-up line can be obtained from
 * buffered_line().  Carriage returns and newlines will have been
 * stripped, and the line will be NUL-terminated.  This buffered line
 * is valid only until the next call to line_buffer() (or to
 * empty_line_buffer()).
 *
 * Note that line buffers use dynamically allocated storage; you
 * should call empty_line_buffer() before freeing a @c struct @c
 * line_buffer.
 */
int line_buffer ( struct line_buffer *linebuf, const char *data, size_t len ) {
	size_t consume = 0;
	size_t copy = 0;
	size_t new_len;
	char *new_data;
	int terminator;

	/* First, handle the termination of the previous line */
	if ( linebuf->skip_terminators ) {
		/* Free buffered string */
		empty_line_buffer ( linebuf );
		/* Skip over any terminators from the end of a previous line */
		for ( ; consume < len ; consume++ ) {
			terminator = terminator_id ( data[consume] );
			if ( ( terminator < 0 ) ||
			     ! ( linebuf->skip_terminators & terminator ) ) {
				linebuf->skip_terminators = 0;
				break;
			}
			linebuf->skip_terminators &= ~terminator;
		}
	}

	/* Scan up to the next terminator, if any */
	for ( ; consume < len ; consume++, copy++ ) {
		if ( terminator_id ( data[consume] ) >= 0 ) {
			linebuf->skip_terminators = -1U;
			break;
		}
	}

	/* Reallocate data buffer and copy in new data */
	new_len = ( linebuf->len + copy );
	new_data = realloc ( linebuf->data, ( new_len + 1 ) );
	if ( ! new_data )
		return -ENOMEM;
	memcpy ( ( new_data + linebuf->len ), ( data + consume - copy ),
		 copy );
	new_data[new_len] = '\0';
	linebuf->data = new_data;
	linebuf->len = new_len;
	return consume;
}
