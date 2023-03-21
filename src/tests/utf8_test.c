/*
 * Copyright (C) 2022 Michael Brown <mbrown@fensystems.co.uk>.
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
 * UTF-8 Unicode encoding tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <string.h>
#include <ipxe/utf8.h>
#include <ipxe/test.h>

/** A UTF-8 accumulation test */
struct utf8_accumulate_test {
	/** UTF-8 byte string */
	const char *bytes;
	/** Expected character sequence */
	const unsigned int *expected;
	/** Length */
	size_t len;
};

/** Define inline data */
#define DATA(...) { __VA_ARGS__ }

/** Define a UTF-8 accumulation test */
#define UTF8_ACCUMULATE( name, BYTES, EXPECTED )			\
	static const char name ## _bytes[] = BYTES;			\
	static const unsigned int name ## _expected[] = EXPECTED;	\
	static struct utf8_accumulate_test name = {			\
		.bytes = name ## _bytes,				\
		.expected = name ## _expected,				\
		.len = ( sizeof ( name ## _expected ) /			\
			 sizeof ( name ## _expected[0] ) ),		\
	};

/** Basic ASCII test */
UTF8_ACCUMULATE ( ascii, "Hello world!",
		  DATA ( 'H', 'e', 'l', 'l', 'o', ' ',
			 'w', 'o', 'r', 'l', 'd', '!' ) );

/** Multi-byte character test */
UTF8_ACCUMULATE ( multibyte, "Héllô wörld 🥳",
		  DATA ( 'H', 0, L'é', 'l', 'l', 0, L'ô', ' ',
			 'w', 0, L'ö', 'r', 'l', 'd', ' ',
			 0, 0, 0, 0x1f973 ) );

/** Stray continuation byte test */
UTF8_ACCUMULATE ( stray_continuation,
		  DATA ( 'a', 0x81, 'b', 0xc3, 0x82, 0x83, 'c' ),
		  DATA ( 'a', 0xfffd, 'b', 0, 0xc2, 0xfffd, 'c' ) );

/** Missing continuation byte test */
UTF8_ACCUMULATE ( missing_continuation,
		  DATA ( 'a', 0xc3, 'b', 0xe1, 0x86, 0xc3, 0x89, 'c' ),
		  DATA ( 'a', 0, 'b', 0, 0, 0, 0xc9, 'c' ) );

/** Illegal two-byte sequence test */
UTF8_ACCUMULATE ( illegal_two,
		  DATA ( 'a', 0xc2, 0x80, 'b', 0xc1, 0xbf, 'c', 0xc0, 0x80,
			 'd' ),
		  DATA ( 'a', 0, 0x80, 'b', 0, 0xfffd, 'c', 0, 0xfffd, 'd' ) );

/** Illegal three-byte sequence test */
UTF8_ACCUMULATE ( illegal_three,
		  DATA ( 'a', 0xe0, 0xa0, 0x80, 'b', 0xe0, 0x9f, 0xbf, 'c',
			 0xe0, 0x80, 0x80, 'd' ),
		  DATA ( 'a', 0, 0, 0x800, 'b', 0, 0, 0xfffd, 'c',
			 0, 0, 0xfffd, 'd' ) );

/** Illegal four-byte sequence test */
UTF8_ACCUMULATE ( illegal_four,
		  DATA ( 'a', 0xf0, 0x90, 0x80, 0x80, 'b', 0xf0, 0x8f, 0xbf,
			 0xbf, 'c', 0xf0, 0x80, 0x80, 0x80, 'd' ),
		  DATA ( 'a', 0, 0, 0, 0x10000, 'b', 0, 0, 0, 0xfffd, 'c',
			 0, 0, 0, 0xfffd, 'd' ) );

/** Illegal overlength sequence test */
UTF8_ACCUMULATE ( illegal_length,
		  DATA ( 'a', 0xf8, 0xbf, 0xbf, 0xbf, 0xbf, 'b', 0xfc, 0xbf,
			 0xbf, 0xbf, 0xbf, 0xbf, 'c', 0xfe, 0xbf, 0xbf, 0xbf,
			 0xbf, 0xbf, 0xbf, 'd', 0xff, 0xbf, 0xbf, 0xbf, 0xbf,
			 0xbf, 0xbf, 0xbf, 'e' ),
		  DATA ( 'a', 0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd, 'b',
			 0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd, 'c',
			 0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd,
			 0xfffd, 'd', 0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd,
			 0xfffd, 0xfffd, 0xfffd, 'e' ) );

/**
 * Report UTF-8 accumulation test result
 *
 * @v test		UTF-8 accumulation test
 * @v file		Test code file
 * @v line		Test code line
 */
static void utf8_accumulate_okx ( struct utf8_accumulate_test *test,
				  const char *file, unsigned int line ) {
	struct utf8_accumulator utf8;
	unsigned int character;
	unsigned int i;

	/* Initialise accumulator */
	memset ( &utf8, 0, sizeof ( utf8 ) );

	/* Test each byte in turn */
	for ( i = 0 ; i < test->len ; i++ ) {
		character = utf8_accumulate ( &utf8, test->bytes[i] );
		DBGC ( test, "UTF8 byte %02x character %02x\n",
		       test->bytes[i], character );
		okx ( character == test->expected[i], file, line );
	}
}
#define utf8_accumulate_ok( test ) \
	utf8_accumulate_okx ( test, __FILE__, __LINE__ )

/**
 * Perform UTF-8 self-test
 *
 */
static void utf8_test_exec ( void ) {

	/* Accumulation tests */
	utf8_accumulate_ok ( &ascii );
	utf8_accumulate_ok ( &multibyte );
	utf8_accumulate_ok ( &stray_continuation );
	utf8_accumulate_ok ( &missing_continuation );
	utf8_accumulate_ok ( &illegal_two );
	utf8_accumulate_ok ( &illegal_three );
	utf8_accumulate_ok ( &illegal_four );
	utf8_accumulate_ok ( &illegal_length );
}

/** UTF-8 self-test */
struct self_test utf8_test __self_test = {
	.name = "utf8",
	.exec = utf8_test_exec,
};
