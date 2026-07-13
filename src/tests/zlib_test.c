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

#include <ipxe/zlib.h>
#include <ipxe/test.h>
#include "archive_test.h"

/** "Hello world" */
ARCHIVE_TEST ( hello_world, &zlib_image_type, "hello.z", NULL, "hello",
	ARCHIVE ( 0x78, 0x9c, 0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0x57, 0x28, 0xcf,
		  0x2f, 0xca, 0x49, 0x01, 0x00, 0x18, 0xab, 0x04, 0x3d ),
	EXPECTED ( "Hello world" ) );

/**
 * Perform zlib self-test
 *
 */
static void zlib_test_exec ( void ) {

	archive_ok ( &hello_world );
}

/** zlib self-test */
struct self_test zlib_test __self_test = {
	.name = "zlib",
	.exec = zlib_test_exec,
};
