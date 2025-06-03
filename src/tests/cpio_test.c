/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
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
 * CPIO self-tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <stdlib.h>
#include <string.h>
#include <ipxe/cpio.h>
#include <ipxe/test.h>

/** A CPIO test */
struct cpio_test {
	/** Test name */
	const char *name;
	/** Image length */
	size_t len;
	/** Image command line */
	const char *cmdline;
	/** Expected CPIO headers */
	const uint8_t *expected;
	/** Length of expected CPIO headers */
	size_t expected_len;
	/** Expected number of CPIO headers */
	unsigned int expected_count;
};

/** Define an expected CPIO header */
#define CPIO_HEADER( mode, filesize, namesize, pname )			\
     "070701" "00000000" mode "00000000" "00000000" "00000001"		\
     "00000000" filesize "00000000" "00000000" "00000000" "00000000"	\
     namesize "00000000" pname

/** Define a one-byte padding */
#define PAD1 "\0"

/** Define a two-byte padding */
#define PAD2 "\0\0"

/** Define a three-byte padding */
#define PAD3 "\0\0\0"

/** Define four-byte padding */
#define PAD4 "\0\0\0\0"

/** Define a CPIO test */
#define CPIO_TEST( NAME, LEN, CMDLINE, COUNT, EXPECTED )		\
	static const uint8_t NAME ## _expected[] = EXPECTED;		\
	static struct cpio_test NAME = {				\
		.name = #NAME,						\
		.len = LEN,						\
		.cmdline = CMDLINE,					\
		.expected = NAME ## _expected,				\
		.expected_len = ( sizeof ( NAME ## _expected )		\
				  - 1 /* NUL */ ),			\
		.expected_count = COUNT,				\
	};

/**
 * Report a CPIO test result
 *
 * @v test		CPIO test
 * @v file		Test code file
 * @v line		Test code line
 */
static void cpio_okx ( struct cpio_test *test, const char *file,
		       unsigned int line ) {
	struct cpio_header cpio;
	struct image *image;
	uint8_t *data;
	size_t len;
	size_t cpio_len;
	unsigned int i;
	unsigned int j;

	DBGC ( test, "CPIO len %#zx cmdline \"%s\"\n",
	       test->len, test->cmdline );
	DBGC2_HDA ( test, 0, test->expected, test->expected_len );

	/* Sanity check */
	okx ( ( test->expected_len % CPIO_ALIGN ) == 0, file, line );

	/* Construct dummy image */
	image = alloc_image ( NULL );
	okx ( image != NULL, file, line );
	okx ( image_set_name ( image, test->name ) == 0, file, line );
	okx ( image_set_len ( image, test->len ) == 0, file, line );
	okx ( image_set_cmdline ( image, test->cmdline ) == 0, file, line );

	/* Calculate length of CPIO headers */
	len = 0;
	for ( i = 0 ; ( cpio_len = cpio_header ( image, i, &cpio ) ) ; i++ ) {
		okx ( cpio_len >= sizeof ( cpio ), file, line );
		len += ( cpio_len + cpio_pad_len ( cpio_len ) );
		okx ( cpio_pad_len ( cpio_len ) > 0, file, line );
		okx ( ( len % CPIO_ALIGN ) == 0, file, line );
	}
	okx ( i == test->expected_count, file, line );
	okx ( len == test->expected_len, file, line );

	/* Allocate space for CPIO headers */
	data = zalloc ( len );
	okx ( data != NULL, file, line );

	/* Construct CPIO headers */
	len = 0;
	for ( i = 0 ; ( cpio_len = cpio_header ( image, i, &cpio ) ) ; i++ ) {
		memcpy ( ( data + len ), &cpio, sizeof ( cpio ) );
		memcpy ( ( data + len + sizeof ( cpio ) ), cpio_name ( image ),
			 ( cpio_len - sizeof ( cpio ) ) );
		DBGC ( test, "CPIO hdr %d: ", i );
		for ( j = 0 ; j < cpio_len ; j++ ) {
			if ( ( j <= sizeof ( cpio ) && ! ( ( j + 2 ) % 8 ) ) )
				DBGC ( test, " " );
			DBGC ( test, "%c", data[ len + j ] );
		}
		DBGC ( test, "\n" );
		len += ( cpio_len + cpio_pad_len ( cpio_len ) );
	}
	okx ( i == test->expected_count, file, line );
	okx ( len == test->expected_len, file, line );

	/* Verify constructed CPIO headers */
	DBGC2_HDA ( test, 0, data, len );
	okx ( memcmp ( data, test->expected, test->expected_len ) == 0,
	      file, line );

	/* Free constructed headers */
	free ( data );

	/* Drop reference to dummy image */
	image_put ( image );
}
#define cpio_ok( test ) cpio_okx ( test, __FILE__, __LINE__ )

/* Image with no command line */
CPIO_TEST ( no_cmdline, 42, NULL, 0, "" );

/* Image with empty command line */
CPIO_TEST ( empty_cmdline, 154, "", 0, "" );

/* All slashes */
CPIO_TEST ( all_slashes, 64, "////", 0, "" );

/* Simple filename */
CPIO_TEST ( simple, 0x69, "wimboot", 1,
	    CPIO_HEADER ( "000081a4", "00000069", "00000008",
			  "wimboot" PAD3 ) );

/* Initial slash */
CPIO_TEST ( init_slash, 0x273, "/wimboot", 1,
	    CPIO_HEADER ( "000081a4", "00000273", "00000009",
			  "/wimboot" PAD2 ) );

/* Initial slashes */
CPIO_TEST ( init_slashes, 0x94, "///initscript", 1,
	    CPIO_HEADER ( "000081a4", "00000094", "0000000e",
			  "///initscript" PAD1 ) );

/* Full path */
CPIO_TEST ( path, 0x341, "/usr/share/oem/config.ign", 1,
	    CPIO_HEADER ( "000081a4", "00000341", "0000001a",
			  "/usr/share/oem/config.ign" PAD1 ) );

/* Full path, mkdir=0 */
CPIO_TEST ( path_mkdir_0, 0x341, "/usr/share/oem/config.ign mkdir=0", 1,
	    CPIO_HEADER ( "000081a4", "00000341", "0000001a",
			  "/usr/share/oem/config.ign" PAD1 ) );

/* Full path, mkdir=1 */
CPIO_TEST ( path_mkdir_1, 0x341, "/usr/share/oem/config.ign mkdir=1", 2,
	    CPIO_HEADER ( "000041ed", "00000000", "0000000f",
			  "/usr/share/oem" PAD4 )
	    CPIO_HEADER ( "000081a4", "00000341", "0000001a",
			  "/usr/share/oem/config.ign" PAD1 ) );

/* Full path, mkdir=2 */
CPIO_TEST ( path_mkdir_2, 0x341, "/usr/share/oem/config.ign mkdir=2", 3,
	    CPIO_HEADER ( "000041ed", "00000000", "0000000b",
			  "/usr/share" PAD4 )
	    CPIO_HEADER ( "000041ed", "00000000", "0000000f",
			  "/usr/share/oem" PAD4 )
	    CPIO_HEADER ( "000081a4", "00000341", "0000001a",
			  "/usr/share/oem/config.ign" PAD1 ) );

/* Full path, mkdir=-1 */
CPIO_TEST ( path_mkdir_all, 0x341, "/usr/share/oem/config.ign mkdir=-1", 4,
	    CPIO_HEADER ( "000041ed", "00000000", "00000005",
			  "/usr" PAD2 )
	    CPIO_HEADER ( "000041ed", "00000000", "0000000b",
			  "/usr/share" PAD4 )
	    CPIO_HEADER ( "000041ed", "00000000", "0000000f",
			  "/usr/share/oem" PAD4 )
	    CPIO_HEADER ( "000081a4", "00000341", "0000001a",
			  "/usr/share/oem/config.ign" PAD1 ) );

/* Simple directory */
CPIO_TEST ( dir, 0, "/opt/", 1,
	    CPIO_HEADER ( "000041ed", "00000000", "00000005",
			  "/opt" PAD2 ) );

/* Directory tree */
CPIO_TEST ( tree, 0, "/opt/oem/scripts/ mkdir=-1", 3,
	    CPIO_HEADER ( "000041ed", "00000000", "00000005",
			  "/opt" PAD2 )
	    CPIO_HEADER ( "000041ed", "00000000", "00000009",
			  "/opt/oem" PAD2 )
	    CPIO_HEADER ( "000041ed", "00000000", "00000011",
			  "/opt/oem/scripts" PAD2 ) );

/* Custom mode */
CPIO_TEST ( mode, 39, "/sbin/init mode=755", 1,
	    CPIO_HEADER ( "000081ed", "00000027", "0000000b",
			  "/sbin/init" PAD4 ) );

/* Chaos */
CPIO_TEST ( chaos, 73, "///etc//init.d///runthings mode=700 mkdir=99", 3,
	    CPIO_HEADER ( "000041ed", "00000000", "00000007",
			  "///etc" PAD4 )
	    CPIO_HEADER ( "000041ed", "00000000", "0000000f",
			  "///etc//init.d" PAD4 )
	    CPIO_HEADER ( "000081c0", "00000049", "0000001b",
			  "///etc//init.d///runthings" PAD4 ) );

/**
 * Perform CPIO self-test
 *
 */
static void cpio_test_exec ( void ) {

	cpio_ok ( &no_cmdline );
	cpio_ok ( &empty_cmdline );
	cpio_ok ( &all_slashes );
	cpio_ok ( &simple );
	cpio_ok ( &init_slash );
	cpio_ok ( &init_slashes );
	cpio_ok ( &path );
	cpio_ok ( &path_mkdir_0 );
	cpio_ok ( &path_mkdir_1 );
	cpio_ok ( &path_mkdir_2 );
	cpio_ok ( &path_mkdir_all );
	cpio_ok ( &dir );
	cpio_ok ( &tree );
	cpio_ok ( &mode );
	cpio_ok ( &chaos );
}

/** CPIO self-test */
struct self_test cpio_test __self_test = {
	.name = "cpio",
	.exec = cpio_test_exec,
};
