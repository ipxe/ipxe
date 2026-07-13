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

#include <ipxe/gzip.h>
#include <ipxe/test.h>
#include "archive_test.h"

/** "Hello world" */
ARCHIVE_TEST ( hello_world, &gzip_image_type, "hw.gz", NULL, "hw",
	ARCHIVE ( 0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
		  0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0x57, 0x28, 0xcf, 0x2f, 0xca,
		  0x49, 0x01, 0x00, 0x52, 0x9e, 0xd6, 0x8b, 0x0b, 0x00, 0x00,
		  0x00 ),
	EXPECTED ( "Hello world" ) );

/** "Hello filename" */
ARCHIVE_TEST ( hello_filename, &gzip_image_type, "hwname.gz", NULL, "hwname",
	ARCHIVE ( 0x1f, 0x8b, 0x08, 0x08, 0xeb, 0x5b, 0x96, 0x60, 0x00, 0x03,
		  0x68, 0x77, 0x2e, 0x74, 0x78, 0x74, 0x00, 0xf3, 0x48, 0xcd,
		  0xc9, 0xc9, 0x57, 0x48, 0xcb, 0xcc, 0x49, 0xcd, 0x4b, 0xcc,
		  0x4d, 0x05, 0x00, 0x69, 0x37, 0x25, 0x3c, 0x0e, 0x00, 0x00,
		  0x00 ),
	EXPECTED ( "Hello filename" ) );

/** "Hello assorted headers" */
ARCHIVE_TEST ( hello_headers, &gzip_image_type, "hwhdrs.gz", NULL, "hwhdrs",
	ARCHIVE ( 0x1f, 0x8b, 0x08, 0x1c, 0x11, 0x5c, 0x96, 0x60, 0x00, 0x03,
		  0x05, 0x00, 0x41, 0x70, 0x01, 0x00, 0x0d, 0x68, 0x77, 0x2e,
		  0x74, 0x78, 0x74, 0x00, 0x2f, 0x2f, 0x77, 0x68, 0x79, 0x3f,
		  0x00, 0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0x57, 0x48, 0x2c, 0x2e,
		  0xce, 0x2f, 0x2a, 0x49, 0x4d, 0x51, 0xc8, 0x48, 0x4d, 0x4c,
		  0x49, 0x2d, 0x2a, 0x06, 0x00, 0x59, 0xa4, 0x19, 0x61, 0x16,
		  0x00, 0x00, 0x00 ),
	EXPECTED ( "Hello assorted headers" ) );

/**
 * Perform gzip self-test
 *
 */
static void gzip_test_exec ( void ) {

	archive_ok ( &hello_world );
	archive_ok ( &hello_filename );
	archive_ok ( &hello_headers );
}

/** gzip self-test */
struct self_test gzip_test __self_test = {
	.name = "gzip",
	.exec = gzip_test_exec,
};
