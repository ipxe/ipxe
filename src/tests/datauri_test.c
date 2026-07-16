/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
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

/** @file
 *
 * Data URI tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <string.h>
#include <ipxe/uri.h>
#include <ipxe/datauri.h>
#include <ipxe/test.h>

/** Define inline data */
#define DATA(...) { __VA_ARGS__ }

/**
 * Report result of data URI test
 *
 * @v uristring		URI string
 * @v expected		Expected parsed data
 * @v expected_len	Length of expected parsed data
 * @v file		Test code file
 * @v line		Test code line
 */
static void datauri_okx ( const char *uristring, const void *expected,
			  size_t expected_len, const char *file,
			  unsigned int line ) {
	uint8_t actual[ strlen ( uristring ) /* more than enough */ ];
	struct uri *uri;
	int len;

	/* Parse URI */
	uri = parse_uri ( uristring );
	okx ( uri != NULL, file, line );
	okx ( uri->scheme != NULL, file, line );
	okx ( uri->opaque != NULL, file, line );
	okx ( strcmp ( uri->scheme, "data" ) == 0, file, line );

	/* Sanity check */
	okx ( datauri_max_len ( uri ) <= sizeof ( actual ), file, line );
	okx ( datauri_max_len ( uri ) >= expected_len, file, line );

	/* Parse data URI */
	len = datauri_parse ( uri, actual );
	okx ( len >= 0, file, line );
	okx ( ( ( size_t ) len ) == expected_len, file, line );
	okx ( memcmp ( actual, expected, expected_len ) == 0, file, line );

	/* Drop reference */
	uri_put ( uri );
}
#define datauri_ok( uristring, EXPECTED ) do {				\
	static const uint8_t expected[] = EXPECTED;			\
	datauri_okx ( uristring, expected, sizeof ( expected ),		\
		      __FILE__, __LINE__ );				\
	} while ( 0 )

/**
 * Report result of data URI failure test
 *
 * @v uristring		URI string
 * @v file		Test code file
 * @v line		Test code line
 */
static void datauri_fail_okx ( const char *uristring, const char *file,
			       unsigned int line ) {
	uint8_t actual[ strlen ( uristring ) /* more than enough */ ];
	struct uri *uri;
	int len;

	/* Parse URI */
	uri = parse_uri ( uristring );
	okx ( uri != NULL, file, line );
	okx ( uri->scheme != NULL, file, line );
	okx ( uri->opaque != NULL, file, line );
	okx ( strcmp ( uri->scheme, "data" ) == 0, file, line );

	/* Sanity check */
	okx ( datauri_max_len ( uri ) <= sizeof ( actual ), file, line );

	/* Parse data URI */
	len = datauri_parse ( uri, actual );
	okx ( len < 0, file, line );

	/* Drop reference */
	uri_put ( uri );
}
#define datauri_fail_ok( uristring ) \
	datauri_fail_okx ( uristring, __FILE__, __LINE__ )

/**
 * Perform data URI self-test
 *
 */
static void datauri_test_exec ( void ) {

	/* Empty data */
	datauri_ok ( "data:,", DATA() );
	datauri_ok ( "data:;base64,", DATA() );
	datauri_ok ( "data:text/plain,", DATA() );

	/* Simple values */
	datauri_ok ( "data:,hello%20world%21",
		     DATA ( 'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l',
			    'd', '!' ) );
	datauri_ok ( "data:text/plain,simple",
		     DATA ( 's', 'i', 'm', 'p', 'l', 'e' ) );
	datauri_ok ( "data:text/plain;charset=US-ASCII;base64,ZW5jb2RlZA==",
		     DATA ( 'e', 'n', 'c', 'o', 'd', 'e', 'd' ) );
	datauri_ok ( "data:application/octet-string;base64,d2l0aABudWw=",
		     DATA ( 'w', 'i', 't', 'h', 0x00, 'n', 'u', 'l' ) );

	/* Invalid values */
	datauri_fail_ok ( "data:" );
	datauri_fail_ok ( "data:;base64,INVALID===" );
}

/** Data URI self-test */
struct self_test datauri_test __self_test = {
	.name = "datauri",
	.exec = datauri_test_exec,
};
