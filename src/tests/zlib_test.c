/*
 * Copyright (C) 2021 Michael Brown <mbrown@fensystems.co.uk>.
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
 * zlib image tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <stdint.h>
#include <ipxe/image.h>
#include <ipxe/zlib.h>
#include <ipxe/test.h>

/** A zlib test */
struct zlib_test {
	/** Compressed filename */
	const char *compressed_name;
	/** Compressed data */
	const void *compressed;
	/** Length of compressed data */
	size_t compressed_len;
	/** Expected uncompressed name */
	const char *expected_name;
	/** Expected uncompressed data */
	const void *expected;
	/** Length of expected uncompressed data */
	size_t expected_len;
};

/** Define inline data */
#define DATA(...) { __VA_ARGS__ }

/** Define a zlib test */
#define ZLIB( name, COMPRESSED, EXPECTED )				\
	static const uint8_t name ## _compressed[] = COMPRESSED;	\
	static const uint8_t name ## _expected[] = EXPECTED;		\
	static struct zlib_test name = {				\
		.compressed_name = #name ".z",				\
		.compressed = name ## _compressed,			\
		.compressed_len = sizeof ( name ## _compressed ),	\
		.expected_name = #name,					\
		.expected = name ## _expected,				\
		.expected_len = sizeof ( name ## _expected ),		\
	};

/** "Hello world" */
ZLIB ( hello_world,
       DATA ( 0x78, 0x9c, 0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0x57, 0x28, 0xcf,
	      0x2f, 0xca, 0x49, 0x01, 0x00, 0x18, 0xab, 0x04, 0x3d ),
       DATA ( 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72, 0x6c,
	      0x64 ) );

/**
 * Report zlib test result
 *
 * @v test		zlib test
 * @v file		Test code file
 * @v line		Test code line
 */
static void zlib_okx ( struct zlib_test *test, const char *file,
		       unsigned int line ) {
	struct image *image;
	struct image *extracted;

	/* Construct compressed image */
	image = image_memory ( test->compressed_name, test->compressed,
			       test->compressed_len );
	okx ( image != NULL, file, line );
	okx ( image->len == test->compressed_len, file, line );

	/* Check type detection */
	okx ( image->type == &zlib_image_type, file, line );

	/* Extract archive image */
	okx ( image_extract ( image, NULL, &extracted ) == 0, file, line );

	/* Verify extracted image content */
	okx ( extracted->len == test->expected_len, file, line );
	okx ( memcmp ( extracted->data, test->expected,
		       test->expected_len ) == 0, file, line );

	/* Verify extracted image name */
	okx ( strcmp ( extracted->name, test->expected_name ) == 0,
	      file, line );

	/* Unregister images */
	unregister_image ( extracted );
	unregister_image ( image );
}
#define zlib_ok( test ) zlib_okx ( test, __FILE__, __LINE__ )

/**
 * Perform zlib self-test
 *
 */
static void zlib_test_exec ( void ) {

	zlib_ok ( &hello_world );
}

/** zlib self-test */
struct self_test zlib_test __self_test = {
	.name = "zlib",
	.exec = zlib_test_exec,
};
