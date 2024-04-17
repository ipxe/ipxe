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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <ipxe/keys.h>
#include <ipxe/editstring.h>

/** @file
 *
 * Editable strings
 *
 */

static __attribute__ (( nonnull ( 1 ) )) int
insert_delete ( struct edit_string *string, size_t delete_len,
		const char *insert_text );
static __nonnull int insert_character ( struct edit_string *string,
					unsigned int character );
static __nonnull void delete_character ( struct edit_string *string );
static __nonnull void backspace ( struct edit_string *string );
static __nonnull void previous_word ( struct edit_string *string );
static __nonnull void kill_word ( struct edit_string *string );
static __nonnull void kill_sol ( struct edit_string *string );
static __nonnull void kill_eol ( struct edit_string *string );

/**
 * Insert and/or delete text within an editable string
 *
 * @v string		Editable string
 * @v delete_len	Length of text to delete from current cursor position
 * @v insert_text	Text to insert at current cursor position, or NULL
 * @ret rc		Return status code
 */
static int insert_delete ( struct edit_string *string, size_t delete_len,
			   const char *insert_text ) {
	size_t old_len, max_delete_len, move_len, insert_len, new_len;
	char *buf;
	char *tmp;

	/* Prepare edit history */
	string->mod_start = string->cursor;
	string->mod_end = string->cursor;

	/* Calculate lengths */
	buf = *(string->buf);
	old_len = ( buf ? strlen ( buf ) : 0 );
	assert ( string->cursor <= old_len );
	max_delete_len = ( old_len - string->cursor );
	if ( delete_len > max_delete_len )
		delete_len = max_delete_len;
	move_len = ( max_delete_len - delete_len );
	insert_len = ( insert_text ? strlen ( insert_text ) : 0 );
	new_len = ( old_len - delete_len + insert_len );

	/* Delete existing text */
	memmove ( ( buf + string->cursor ),
		  ( buf + string->cursor + delete_len ), move_len );

	/* Reallocate string, ignoring failures if shrinking */
	tmp = realloc ( buf, ( new_len + 1 /* NUL */ ) );
	if ( tmp ) {
		buf = tmp;
		*(string->buf) = buf;
	} else if ( ( new_len > old_len ) || ( ! buf ) ) {
		return -ENOMEM;
	}

	/* Create space for inserted text */
	memmove ( ( buf + string->cursor + insert_len ),
		  ( buf + string->cursor ), move_len );

	/* Copy inserted text to cursor position */
	memcpy ( ( buf + string->cursor ), insert_text, insert_len );
	string->cursor += insert_len;

	/* Terminate string */
	buf[new_len] = '\0';

	/* Update edit history */
	string->mod_end = ( ( new_len > old_len ) ? new_len : old_len );

	return 0;
}

/**
 * Insert character at current cursor position
 *
 * @v string		Editable string
 * @v character		Character to insert
 * @ret rc		Return status code
 */
static int insert_character ( struct edit_string *string,
			      unsigned int character ) {
	char insert_text[2] = { character, '\0' };

	return insert_delete ( string, 0, insert_text );
}

/**
 * Delete character at current cursor position
 *
 * @v string		Editable string
 */
static void delete_character ( struct edit_string *string ) {
	int rc;

	rc = insert_delete ( string, 1, NULL );
	assert ( ( rc == 0 ) || ( *(string->buf) == NULL ) );
}

/**
 * Delete character to left of current cursor position
 *
 * @v string		Editable string
 */
static void backspace ( struct edit_string *string ) {

	if ( string->cursor > 0 ) {
		string->cursor--;
		delete_character ( string );
	}
}

/**
 * Move to start of previous word
 *
 * @v string		Editable string
 */
static void previous_word ( struct edit_string *string ) {
	const char *buf = *(string->buf);
	size_t cursor = string->cursor;

	while ( cursor && isspace ( buf[ cursor - 1 ] ) ) {
		cursor--;
	}
	while ( cursor && ( ! isspace ( buf[ cursor - 1 ] ) ) ) {
		cursor--;
	}
	string->cursor = cursor;
}

/**
 * Delete to end of previous word
 *
 * @v string		Editable string
 */
static void kill_word ( struct edit_string *string ) {
	size_t old_cursor = string->cursor;
	int rc;

	previous_word ( string );
	rc = insert_delete ( string, ( old_cursor - string->cursor ), NULL );
	assert ( ( rc == 0 ) || ( *(string->buf) == NULL ) );
}

/**
 * Delete to start of line
 *
 * @v string		Editable string
 */
static void kill_sol ( struct edit_string *string ) {
	size_t old_cursor = string->cursor;
	int rc;

	string->cursor = 0;
	rc = insert_delete ( string, old_cursor, NULL );
	assert ( ( rc == 0 ) || ( *(string->buf) == NULL ) );
}

/**
 * Delete to end of line
 *
 * @v string		Editable string
 */
static void kill_eol ( struct edit_string *string ) {
	int rc;

	rc = insert_delete ( string, ~( ( size_t ) 0 ), NULL );
	assert ( ( rc == 0 ) || ( *(string->buf) == NULL ) );
}

/**
 * Replace editable string
 *
 * @v string		Editable string
 * @v replacement	Replacement string, or NULL to empty the string
 * @ret rc		Return status code
 *
 * Replace the entire content of the editable string and update the
 * edit history to allow the caller to bring the display into sync
 * with the string content.
 *
 * This function does not itself update the display in any way.
 *
 * Upon success, the string buffer is guaranteed to be non-NULL (even
 * if the replacement string is NULL or empty).
 *
 * Errors may safely be ignored if it is deemed that subsequently
 * failing to update the display will provide sufficient feedback to
 * the user.
 */
int replace_string ( struct edit_string *string, const char *replacement ) {

	string->cursor = 0;
	return insert_delete ( string, ~( ( size_t ) 0 ), replacement );
}

/**
 * Edit editable string
 *
 * @v string		Editable string
 * @v key		Key pressed by user
 * @ret key		Key returned to application, zero, or negative error
 *
 * Handles keypresses and updates the content of the editable string.
 * Basic line editing facilities (delete/insert/cursor) are supported.
 * If edit_string() understands and uses the keypress it will return
 * zero, otherwise it will return the original key.
 *
 * The string's edit history will be updated to allow the caller to
 * efficiently bring the display into sync with the string content.
 *
 * This function does not itself update the display in any way.
 *
 * Errors may safely be ignored if it is deemed that subsequently
 * failing to update the display will provide sufficient feedback to
 * the user.
 */
int edit_string ( struct edit_string *string, int key ) {
	const char *buf = *(string->buf);
	size_t len = ( buf ? strlen ( buf ) : 0 );
	int retval = 0;

	/* Prepare edit history */
	string->last_cursor = string->cursor;
	string->mod_start = string->cursor;
	string->mod_end = string->cursor;

	/* Interpret key */
	if ( ( key >= 0x20 ) && ( key <= 0x7e ) ) {
		/* Printable character; insert at current position */
		retval = insert_character ( string, key );
	} else switch ( key ) {
	case KEY_BACKSPACE:
		/* Backspace */
		backspace ( string );
		break;
	case KEY_DC:
	case CTRL_D:
		/* Delete character */
		delete_character ( string );
		break;
	case CTRL_W:
		/* Delete word */
		kill_word ( string );
		break;
	case CTRL_U:
		/* Delete to start of line */
		kill_sol ( string );
		break;
	case CTRL_K:
		/* Delete to end of line */
		kill_eol ( string );
		break;
	case KEY_HOME:
	case CTRL_A:
		/* Start of line */
		string->cursor = 0;
		break;
	case KEY_END:
	case CTRL_E:
		/* End of line */
		string->cursor = len;
		break;
	case KEY_LEFT:
	case CTRL_B:
		/* Cursor left */
		if ( string->cursor > 0 )
			string->cursor--;
		break;
	case KEY_RIGHT:
	case CTRL_F:
		/* Cursor right */
		if ( string->cursor < len )
			string->cursor++;
		break;
	default:
		retval = key;
		break;
	}

	return retval;
}
