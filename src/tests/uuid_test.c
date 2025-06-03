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
 * UUID tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <string.h>
#include <byteswap.h>
#include <ipxe/uuid.h>
#include <ipxe/test.h>

/** Define an inline UUID value */
#define UUID( A, B, C, D, E0, E1, E2, E3, E4, E5 ) {		\
		.a = htonl ( A ),				\
		.b = htons ( B ),				\
		.c = htons ( C ),				\
		.d = htons ( D ),				\
		.e = { E0, E1, E2, E3, E4, E5 },		\
	}

/**
 * Report a uuid_ntoa() test result
 *
 * @v uuid		UUID
 * @v text		Expected textual representation
 * @v file		Test code file
 * @v line		Test code line
 */
static void uuid_ntoa_okx ( const union uuid *uuid, const char *text,
			    const char *file, unsigned int line ) {
	const char *actual;

	/* Format address */
	actual = uuid_ntoa ( uuid );
	DBG ( "uuid_ntoa ( %08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x ) = "
	      "\"%s\"\n", ntohl ( uuid->canonical.a ),
	      ntohs ( uuid->canonical.b ), ntohs ( uuid->canonical.c ),
	      ntohs ( uuid->canonical.d ), uuid->canonical.e[0],
	      uuid->canonical.e[1], uuid->canonical.e[2], uuid->canonical.e[3],
	      uuid->canonical.e[4], uuid->canonical.e[5], actual );
	okx ( strcmp ( actual, text ) == 0, file, line );
}
#define uuid_ntoa_ok( value, text ) do {				\
	static const union uuid uuid = {				\
		.canonical = value,					\
	};								\
	uuid_ntoa_okx ( &uuid, text, __FILE__, __LINE__ );		\
	} while ( 0 )

/**
 * Report a uuid_aton() test result
 *
 * @v text		Textual representation
 * @v uuid		Expected UUID
 * @v file		Test code file
 * @v line		Test code line
 */
static void uuid_aton_okx ( const char *text, const union uuid *uuid,
			    const char *file, unsigned int line ) {
	union uuid actual;

	/* Parse address */
	okx ( uuid_aton ( text, &actual ) == 0, file, line );
	DBG ( "uuid_aton ( \"%s\" ) = %s\n", text, uuid_ntoa ( &actual ) );
	okx ( memcmp ( &actual, uuid, sizeof ( actual ) ) == 0, file, line );
};
#define uuid_aton_ok( text, value ) do {				\
	static const union uuid uuid = {				\
		.canonical = value,					\
	};								\
	uuid_aton_okx ( text, &uuid, __FILE__, __LINE__ );		\
	} while ( 0 )

/**
 * Report a uuid_aton() failure test result
 *
 * @v text		Textual representation
 * @v file		Test code file
 * @v line		Test code line
 */
static void uuid_aton_fail_okx ( const char *text, const char *file,
				 unsigned int line ) {
	union uuid actual;

	/* Attempt to parse address */
	okx ( uuid_aton ( text, &actual ) != 0, file, line );
}
#define uuid_aton_fail_ok( text ) \
	uuid_aton_fail_okx ( text, __FILE__, __LINE__ )

/**
 * Perform UUID self-tests
 *
 */
static void uuid_test_exec ( void ) {

	/* uuid_ntoa() tests */
	uuid_ntoa_ok ( UUID ( 0x18725ca6, 0xd699, 0x4e4d, 0xb501,
			      0xc3, 0x80, 0x91, 0xd2, 0xa4, 0x33 ),
		       "18725ca6-d699-4e4d-b501-c38091d2a433" );
	uuid_ntoa_ok ( UUID ( 0x1a969b23, 0xc7d5, 0x40fe, 0xb79a,
			      0xc9, 0x2e, 0xa3, 0x4a ,0xb4, 0x5b ),
		       "1a969b23-c7d5-40fe-b79a-c92ea34ab45b" );

	/* uuid_aton() tests */
	uuid_aton_ok ( "62b907a8-e1a7-460e-82f7-667d84270c84",
		       UUID ( 0x62b907a8, 0xe1a7, 0x460e, 0x82f7,
			      0x66, 0x7d, 0x84, 0x27, 0x0c, 0x84 ) );
	uuid_aton_ok ( "F5D0349C-EF7C-4AD4-B40B-FC2E522A7327",
		       UUID ( 0xf5d0349c, 0xef7c, 0x4ad4, 0xb40b,
			      0xfc, 0x2e, 0x52, 0x2a, 0x73, 0x27 ) );
	uuid_aton_ok ( "4edd80ff7b43465589a02b1e7cffa196",
		       UUID ( 0x4edd80ff, 0x7b43, 0x4655, 0x89a0,
			      0x2b, 0x1e, 0x7c, 0xff, 0xa1, 0x96 ) );

	/* uuid_aton() failure tests */
	uuid_aton_fail_ok ( "628d677b-cf38-471e-9ad9-c8a5d9220055b6" );
	uuid_aton_fail_ok ( "5071ca26-fc5f-4580-887a-46d9a103e4" );
	uuid_aton_fail_ok ( "453aee96:0fb5-4aeb-aecd-d060b2121218" );
	uuid_aton_fail_ok ( "1ccb524a-b8b9-4b17-x5e2-7996867edc7d" );
	uuid_aton_fail_ok ( "" );
}

/** UUID self-test */
struct self_test uuid_test __self_test = {
	.name = "uuid",
	.exec = uuid_test_exec,
};
