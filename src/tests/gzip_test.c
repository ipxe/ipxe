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
 * gzip image tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <stdint.h>
#include <string.h>
#include <ipxe/image.h>
#include <ipxe/gzip.h>
#include <ipxe/test.h>

/** A gzip test */
struct gzip_test {
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

/** Define a gzip test */
#define GZIP( name, COMPRESSED, EXPECTED )				\
	static const uint8_t name ## _compressed[] = COMPRESSED;	\
	static const uint8_t name ## _expected[] = EXPECTED;		\
	static struct gzip_test name = {				\
		.compressed_name = #name ".gz",				\
		.compressed = name ## _compressed,			\
		.compressed_len = sizeof ( name ## _compressed ),	\
		.expected_name = #name,					\
		.expected = name ## _expected,				\
		.expected_len = sizeof ( name ## _expected ),		\
	};

/** "Hello world" */
GZIP ( hello_world,
       DATA ( 0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
	      0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0x57, 0x28, 0xcf, 0x2f, 0xca,
	      0x49, 0x01, 0x00, 0x52, 0x9e, 0xd6, 0x8b, 0x0b, 0x00, 0x00,
	      0x00 ),
       DATA ( 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72, 0x6c,
	      0x64 ) );

/** "Hello filename" */
GZIP ( hello_filename,
       DATA ( 0x1f, 0x8b, 0x08, 0x08, 0xeb, 0x5b, 0x96, 0x60, 0x00, 0x03,
	      0x68, 0x77, 0x2e, 0x74, 0x78, 0x74, 0x00, 0xf3, 0x48, 0xcd,
	      0xc9, 0xc9, 0x57, 0x48, 0xcb, 0xcc, 0x49, 0xcd, 0x4b, 0xcc,
	      0x4d, 0x05, 0x00, 0x69, 0x37, 0x25, 0x3c, 0x0e, 0x00, 0x00,
	      0x00 ),
       DATA ( 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x66, 0x69, 0x6c, 0x65,
	      0x6e, 0x61, 0x6d, 0x65 ) );

/** "Hello assorted headers" */
GZIP ( hello_headers,
       DATA ( 0x1f, 0x8b, 0x08, 0x1c, 0x11, 0x5c, 0x96, 0x60, 0x00, 0x03,
	      0x05, 0x00, 0x41, 0x70, 0x01, 0x00, 0x0d, 0x68, 0x77, 0x2e,
	      0x74, 0x78, 0x74, 0x00, 0x2f, 0x2f, 0x77, 0x68, 0x79, 0x3f,
	      0x00, 0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0x57, 0x48, 0x2c, 0x2e,
	      0xce, 0x2f, 0x2a, 0x49, 0x4d, 0x51, 0xc8, 0x48, 0x4d, 0x4c,
	      0x49, 0x2d, 0x2a, 0x06, 0x00, 0x59, 0xa4, 0x19, 0x61, 0x16,
	      0x00, 0x00, 0x00 ),
       DATA ( 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x61, 0x73, 0x73, 0x6f,
	      0x72, 0x74, 0x65, 0x64, 0x20, 0x68, 0x65, 0x61, 0x64, 0x65,
	      0x72, 0x73 ) );

/**
 * Report gzip test result
 *
 * @v test		gzip test
 * @v file		Test code file
 * @v line		Test code line
 */
static void gzip_okx ( struct gzip_test *test, const char *file,
		       unsigned int line ) {
	struct image *image;
	struct image *extracted;

	/* Construct compressed image */
	image = image_memory ( test->compressed_name, test->compressed,
			       test->compressed_len );
	okx ( image != NULL, file, line );
	okx ( image->len == test->compressed_len, file, line );

	/* Check type detection */
	okx ( image->type == &gzip_image_type, file, line );

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
#define gzip_ok( test ) gzip_okx ( test, __FILE__, __LINE__ )

/**
 * Perform gzip self-test
 *
 */
static void gzip_test_exec ( void ) {

	gzip_ok ( &hello_world );
	gzip_ok ( &hello_filename );
	gzip_ok ( &hello_headers );
}

/** gzip self-test */
struct self_test gzip_test __self_test = {
	.name = "gzip",
	.exec = gzip_test_exec,
};
