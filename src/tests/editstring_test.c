/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

/** @file
 *
 * Editable string tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ipxe/keys.h>
#include <ipxe/editstring.h>
#include <ipxe/test.h>

/** An editable string test */
struct editstring_test {
	/** Initial string, or NULL */
	const char *start;
	/** Key sequence */
	const int *keys;
	/** Length of key sequence */
	unsigned int count;
	/** Expected result */
	const char *expected;
};

/** Define an inline key sequence */
#define KEYS(...) { __VA_ARGS__ }

/** Define an editable string test */
#define EDITSTRING_TEST( name, START, EXPECTED, KEYS )			\
	static const int name ## _keys[] = KEYS;			\
	static struct editstring_test name = {				\
		.start = START,						\
		.keys = name ## _keys,					\
		.count = ( sizeof ( name ## _keys ) /			\
			   sizeof ( name ## _keys[0] ) ),		\
		.expected = EXPECTED,					\
	};

/* Simple typing */
EDITSTRING_TEST ( simple, "", "hello world!",
		  KEYS ( 'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l',
			 'd', '!' ) );

/* Simple typing from a NULL starting value */
EDITSTRING_TEST ( simple_null, NULL, "hi there",
		  KEYS ( 'h', 'i', ' ', 't', 'h', 'e', 'r', 'e' ) );

/* Insertion */
EDITSTRING_TEST ( insert, "in middle", "in the middle",
		  KEYS ( KEY_LEFT, KEY_LEFT, KEY_LEFT, KEY_LEFT, KEY_LEFT,
			 KEY_LEFT, 't', 'h', 'e', ' ' ) );

/* Backspace at end */
EDITSTRING_TEST ( backspace_end, "byebye", "bye",
		  KEYS ( KEY_BACKSPACE, KEY_BACKSPACE, KEY_BACKSPACE ) );

/* Backspace of whole string */
EDITSTRING_TEST ( backspace_all, "abc", "",
		  KEYS ( KEY_BACKSPACE, KEY_BACKSPACE, KEY_BACKSPACE ) );

/* Backspace of empty string */
EDITSTRING_TEST ( backspace_empty, NULL, "", KEYS ( KEY_BACKSPACE ) );

/* Backspace beyond start of string */
EDITSTRING_TEST ( backspace_beyond, "too far", "",
		  KEYS ( KEY_BACKSPACE, KEY_BACKSPACE, KEY_BACKSPACE,
			 KEY_BACKSPACE, KEY_BACKSPACE, KEY_BACKSPACE,
			 KEY_BACKSPACE, KEY_BACKSPACE, KEY_BACKSPACE ) );

/* Deletion of character at cursor via DEL */
EDITSTRING_TEST ( delete_dc, "go away", "goaway",
		  KEYS ( KEY_HOME, KEY_RIGHT, KEY_RIGHT, KEY_DC ) );

/* Deletion of character at cursor via Ctrl-D */
EDITSTRING_TEST ( delete_ctrl_d, "not here", "nohere",
		  KEYS ( KEY_LEFT, KEY_LEFT, KEY_LEFT, KEY_LEFT, KEY_LEFT,
			 KEY_LEFT, CTRL_D, CTRL_D ) );

/* Deletion of word at end of string */
EDITSTRING_TEST ( word_end, "remove these two words", "remove these ",
		  KEYS ( CTRL_W, CTRL_W ) );

/* Deletion of word at start of string */
EDITSTRING_TEST ( word_start, "no word", "word",
		  KEYS ( CTRL_A, KEY_RIGHT, KEY_RIGHT, KEY_RIGHT, CTRL_W ) );

/* Deletion of word mid-string */
EDITSTRING_TEST ( word_mid, "delete this word", "delete word",
		  KEYS ( KEY_LEFT, KEY_LEFT, KEY_LEFT, KEY_LEFT, CTRL_W ) );

/* Deletion to start of line */
EDITSTRING_TEST ( sol, "everything must go", "go",
		  KEYS ( KEY_LEFT, KEY_LEFT, CTRL_U ) );

/* Delete to end of line */
EDITSTRING_TEST ( eol, "all is lost", "all",
		  KEYS ( KEY_HOME, KEY_RIGHT, KEY_RIGHT, KEY_RIGHT, CTRL_K ) );

/**
 * Report an editable string test result
 *
 * @v test		Editable string test
 * @v file		Test code file
 * @v line		Test code line
 */
static void editstring_okx ( struct editstring_test *test, const char *file,
			     unsigned int line ) {
	struct edit_string string;
	unsigned int i;
	char *actual;
	int key;

	/* Initialise editable string */
	memset ( &string, 0, sizeof ( string ) );
	actual = NULL;
	init_editstring ( &string, &actual );

	/* Set initial string content */
	okx ( replace_string ( &string, test->start ) == 0, file, line );
	okx ( actual != NULL, file, line );
	okx ( string.cursor == ( test->start ? strlen ( test->start ) : 0 ),
	      file, line );
	DBGC ( test, "Initial string: \"%s\"\n", actual );

	/* Inject keypresses */
	for ( i = 0 ; i < test->count ; i++ ) {
		key = test->keys[i];
		okx ( edit_string ( &string, key ) == 0, file, line );
		okx ( actual != NULL, file, line );
		okx ( string.cursor <= strlen ( actual ), file, line );
		DBGC ( test, "After key %#02x (%c): \"%s\"\n",
		       key, ( isprint ( key ) ? key : '.' ), actual );
	}

	/* Verify result string */
	okx ( strcmp ( actual, test->expected ) == 0, file, line );

	/* Free result string */
	free ( actual );
}
#define editstring_ok( test ) editstring_okx ( test, __FILE__, __LINE__ )

/**
 * Perform editable string self-tests
 *
 */
static void editstring_test_exec ( void ) {

	editstring_ok ( &simple );
	editstring_ok ( &simple_null );
	editstring_ok ( &insert );
	editstring_ok ( &backspace_end );
	editstring_ok ( &backspace_all );
	editstring_ok ( &backspace_empty );
	editstring_ok ( &backspace_beyond );
	editstring_ok ( &delete_dc );
	editstring_ok ( &delete_ctrl_d );
	editstring_ok ( &word_end );
	editstring_ok ( &word_start );
	editstring_ok ( &word_mid );
	editstring_ok ( &sol );
	editstring_ok ( &eol );
}

/** Editable string self-test */
struct self_test editstring_test __self_test = {
	.name = "editstring",
	.exec = editstring_test_exec,
};
