/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
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
 * MS-CHAPv2 authentication self-tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <stdlib.h>
#include <string.h>
#include <ipxe/mschapv2.h>
#include <ipxe/test.h>

/** An MS-CHAPv2 test */
struct mschapv2_test {
	/** Username */
	const char *username;
	/** Password */
	const char *password;
	/** Authenticator challenge */
	const struct mschapv2_challenge *challenge;
	/** Peer challenge */
	const struct mschapv2_challenge *peer;
	/** Expected challenge response */
	const struct mschapv2_response *response;
	/** Expected authenticator response */
	const struct mschapv2_auth *auth;
};

/** Define inline data */
#define DATA(...) { __VA_ARGS__ }

/** Define an MS-CHAPv2 test */
#define MSCHAPV2_TEST( name, USERNAME, PASSWORD, CHALLENGE, PEER,	\
		       RESPONSE, AUTH )					\
	static const struct mschapv2_challenge name ## _challenge = {	\
		.byte = CHALLENGE,					\
	};								\
	static const struct mschapv2_challenge name ## _peer = {	\
		.byte = PEER,						\
	};								\
	static const union {						\
		struct mschapv2_response response;			\
		uint8_t byte[ sizeof ( struct mschapv2_response ) ];	\
	} name ## _response = {						\
		.byte = RESPONSE,					\
	};								\
	static const union {						\
		struct mschapv2_auth auth;				\
		uint8_t byte[ sizeof ( struct mschapv2_auth ) ];	\
	} name ## _auth = {						\
		.byte = AUTH,						\
	};								\
	static struct mschapv2_test name = {				\
		.username = USERNAME,					\
		.password = PASSWORD,					\
		.challenge = &name ## _challenge,			\
		.peer = &name ## _peer,					\
		.response = &name ## _response.response,		\
		.auth = &name ## _auth.auth,				\
	};

/** RFC 2759 section 9.2 test case */
MSCHAPV2_TEST ( rfc2759_test,
		"User", "clientPass",
		DATA ( 0x5b, 0x5d, 0x7c, 0x7d, 0x7b, 0x3f, 0x2f, 0x3e,
		       0x3c, 0x2c, 0x60, 0x21, 0x32, 0x26, 0x26, 0x28 ),
		DATA ( 0x21, 0x40, 0x23, 0x24, 0x25, 0x5e, 0x26, 0x2a,
		       0x28, 0x29, 0x5f, 0x2b, 0x3a, 0x33, 0x7c, 0x7e ),
		DATA ( 0x21, 0x40, 0x23, 0x24, 0x25, 0x5e, 0x26, 0x2a,
		       0x28, 0x29, 0x5f, 0x2b, 0x3a, 0x33, 0x7c, 0x7e,
		       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		       0x82, 0x30, 0x9e, 0xcd, 0x8d, 0x70, 0x8b, 0x5e,
		       0xa0, 0x8f, 0xaa, 0x39, 0x81, 0xcd, 0x83, 0x54,
		       0x42, 0x33, 0x11, 0x4a, 0x3d, 0x85, 0xd6, 0xdf,
		       0x00 ),
		"S=407A5589115FD0D6209F510FE9C04566932CDA56" );

/**
 * Report an MS-CHAPv2 test result
 *
 * @v test		Authentication test
 * @v file		Test code file
 * @v line		Test code line
 */
static void mschapv2_okx ( struct mschapv2_test *test,
			   const char *file, unsigned int line ) {
	struct mschapv2_response response;
	struct mschapv2_auth auth;

	/* Compute challenge response */
	mschapv2_response ( test->username, test->password, test->challenge,
			    test->peer, &response );
	okx ( memcmp ( &response, test->response, sizeof ( response ) ) == 0,
	      file, line );

	/* Compute authenticator response */
	mschapv2_auth ( test->username, test->password, test->challenge,
			test->response, &auth );
	okx ( memcmp ( &auth, test->auth, sizeof ( auth ) ) == 0, file, line );
}
#define mschapv2_ok( test )				\
	mschapv2_okx ( test, __FILE__, __LINE__ )

/**
 * Perform MS-CHAPv2 self-test
 *
 */
static void mschapv2_test_exec ( void ) {

	mschapv2_ok ( &rfc2759_test );
}

/** MS-CHAPv2 self-test */
struct self_test mschapv2_test __self_test = {
	.name = "mschapv2",
	.exec = mschapv2_test_exec,
};
