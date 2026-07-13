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
 * MIME image tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <ipxe/mime.h>
#include <ipxe/test.h>
#include "archive_test.h"

/** "Hello world" */
ARCHIVE_TEST ( hello_world, &mime_image_type, "hw.mime", NULL, "hw",
	ARCHIVE ( "Content-Type: text/x-ipxe; charset=\"utf-8\"\r\n"
		  "MIME-Version: 1.0\r\n"
		  "Content-Transfer-Encoding: base64\r\n"
		  "Content-Disposition: attachment; filename=\"hw.ipxe\"\r\n"
		  "\r\n"
		  "IyFpcHhlCgplY2hvIEhlbGxvIHdvcmxkCnNoZWxsCg==\r\n" ),
	EXPECTED ( "#!ipxe\n"
		   "\n"
		   "echo Hello world\n"
		   "shell\n" ) );

/** Multipart test */
ARCHIVE_TEST ( multipart, &mime_image_type, "user-data", NULL, "user-data",
	ARCHIVE ( "Content-Type: multipart/mixed; "
		      "boundary=\"===============5227682047807177400==\"\r\n"
		  "MIME-Version: 1.0\r\n"
		  "\r\n"
		  "--===============5227682047807177400==\r\n"
		  "Content-Type: text/cloud-config; charset=\"utf-8\"\r\n"
		  "MIME-Version: 1.0\r\n"
		  "Content-Transfer-Encoding: base64\r\n"
		  "Content-Disposition: attachment; filename=\"conf.yml\"\r\n"
		  "\r\n"
		  "LS0tCmNvbmZpZzoKICB0aGluZ3M6CiAgICAtIG9uZQogICAgLSB0d2\r\n"
		  "8KICBvdGhlcnRoaW5nOiAid29vaG9vIgo=\r\n"
		  "\r\n"
		  "--===============5227682047807177400==\r\n"
		  "Content-Type: text/x-ipxe; charset=\"utf-8\"\r\n"
		  "MIME-Version: 1.0\r\n"
		  "Content-Transfer-Encoding: base64\r\n"
		  "Content-Disposition: attachment; filename=\"hw.ipxe\"\r\n"
		  "\r\n"
		  "IyFpcHhlCgplY2hvIEhlbGxvIHdvcmxkCnNoZWxsCg==\r\n"
		  "\r\n"
		  "--===============5227682047807177400==--\r\n" ),
	EXPECTED ( "Content-Type: text/x-ipxe; charset=\"utf-8\"\r\n"
		   "MIME-Version: 1.0\r\n"
		   "Content-Transfer-Encoding: base64\r\n"
		   "Content-Disposition: attachment; filename=\"hw.ipxe\"\r\n"
		   "\r\n"
		   "IyFpcHhlCgplY2hvIEhlbGxvIHdvcmxkCnNoZWxsCg==\r\n"
		   "\r\n" ) );

/**
 * Perform mime self-test
 *
 */
static void mime_test_exec ( void ) {

	archive_ok ( &hello_world );
	archive_ok ( &multipart );
}

/** MIME self-test */
struct self_test mime_test __self_test = {
	.name = "mime",
	.exec = mime_test_exec,
};
