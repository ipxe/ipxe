/*
 * Copyright (C) 2017 Michael Brown <mbrown@fensystems.co.uk>.
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
 * NTLM authentication self-tests
 *
 * The test vectors are taken from the MS-NLMP specification document.
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <stdlib.h>
#include <string.h>
#include <byteswap.h>
#include <ipxe/ntlm.h>
#include <ipxe/test.h>

/** A key generation test */
struct ntlm_key_test {
	/** Domain name (or NULL) */
	const char *domain;
	/** User name (or NULL) */
	const char *username;
	/** Password (or NULL) */
	const char *password;
	/** Expected key */
	struct ntlm_key expected;
};

/** An authentication test */
struct ntlm_authenticate_test {
	/** Domain name (or NULL) */
	const char *domain;
	/** User name (or NULL) */
	const char *username;
	/** Password (or NULL) */
	const char *password;
	/** Workstation (or NULL) */
	const char *workstation;
	/** Nonce */
	struct ntlm_nonce nonce;
	/** Challenge message */
	struct ntlm_challenge *challenge;
	/** Length of Challenge message */
	size_t challenge_len;
	/** Expected Authenticate message */
	struct ntlm_authenticate *expected;
	/** Expected length of Authenticate message */
	size_t expected_len;
};

/** Define inline message data */
#define DATA(...) { __VA_ARGS__ }

/** Define a key generation digest test */
#define KEY_TEST( name, DOMAIN, USERNAME, PASSWORD, EXPECTED )		\
	static struct ntlm_key_test name = {				\
		.domain = DOMAIN,					\
		.username = USERNAME,					\
		.password = PASSWORD,					\
		.expected = {						\
			.raw = EXPECTED,				\
		},							\
	};

/** Define an authentication test */
#define AUTHENTICATE_TEST( name, DOMAIN, USERNAME, PASSWORD,		\
			   WORKSTATION, NONCE, CHALLENGE, EXPECTED )	\
	static const uint8_t name ## _challenge[] = CHALLENGE;		\
	static const uint8_t name ## _expected[] = EXPECTED;		\
	static struct ntlm_authenticate_test name = {			\
		.domain = DOMAIN,					\
		.username = USERNAME,					\
		.password = PASSWORD,					\
		.workstation = WORKSTATION,				\
		.nonce = {						\
			.raw = NONCE,					\
		},							\
		.challenge = ( ( void * ) name ## _challenge ),		\
		.challenge_len = sizeof ( name ## _challenge ),		\
		.expected = ( ( void * ) name ## _expected ),		\
		.expected_len = sizeof ( name ## _expected ),		\
	};

/** NTOWFv2() test from MS-NLMP specification */
KEY_TEST ( msnlmp_ntowfv2, "Domain", "User", "Password",
	DATA ( 0x0c, 0x86, 0x8a, 0x40, 0x3b, 0xfd, 0x7a, 0x93, 0xa3, 0x00,
	       0x1e, 0xf2, 0x2e, 0xf0, 0x2e, 0x3f ) );

/** Authentication test from MS-NLMP specification */
AUTHENTICATE_TEST ( msnlmp_authenticate,
	"Domain", "User", "Password", "COMPUTER",
	DATA ( 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa ),
	DATA ( 0x4e, 0x54, 0x4c, 0x4d, 0x53, 0x53, 0x50, 0x00, 0x02, 0x00,
	       0x00, 0x00, 0x0c, 0x00, 0x0c, 0x00, 0x38, 0x00, 0x00, 0x00,
	       0x33, 0x82, 0x8a, 0xe2, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab,
	       0xcd, 0xef, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x24, 0x00, 0x24, 0x00, 0x44, 0x00, 0x00, 0x00, 0x06, 0x00,
	       0x70, 0x17, 0x00, 0x00, 0x00, 0x0f, 0x53, 0x00, 0x65, 0x00,
	       0x72, 0x00, 0x76, 0x00, 0x65, 0x00, 0x72, 0x00, 0x02, 0x00,
	       0x0c, 0x00, 0x44, 0x00, 0x6f, 0x00, 0x6d, 0x00, 0x61, 0x00,
	       0x69, 0x00, 0x6e, 0x00, 0x01, 0x00, 0x0c, 0x00, 0x53, 0x00,
	       0x65, 0x00, 0x72, 0x00, 0x76, 0x00, 0x65, 0x00, 0x72, 0x00,
	       0x00, 0x00, 0x00, 0x00 ),
	DATA ( 0x4e, 0x54, 0x4c, 0x4d, 0x53, 0x53, 0x50, 0x00, 0x03, 0x00,
	       0x00, 0x00, 0x18, 0x00, 0x18, 0x00, 0x6c, 0x00, 0x00, 0x00,
	       0x54, 0x00, 0x54, 0x00, 0x84, 0x00, 0x00, 0x00, 0x0c, 0x00,
	       0x0c, 0x00, 0x48, 0x00, 0x00, 0x00, 0x08, 0x00, 0x08, 0x00,
	       0x54, 0x00, 0x00, 0x00, 0x10, 0x00, 0x10, 0x00, 0x5c, 0x00,
	       0x00, 0x00, 0x10, 0x00, 0x10, 0x00, 0xd8, 0x00, 0x00, 0x00,
	       0x35, 0x82, 0x88, 0xe2, 0x05, 0x01, 0x28, 0x0a, 0x00, 0x00,
	       0x00, 0x0f, 0x44, 0x00, 0x6f, 0x00, 0x6d, 0x00, 0x61, 0x00,
	       0x69, 0x00, 0x6e, 0x00, 0x55, 0x00, 0x73, 0x00, 0x65, 0x00,
	       0x72, 0x00, 0x43, 0x00, 0x4f, 0x00, 0x4d, 0x00, 0x50, 0x00,
	       0x55, 0x00, 0x54, 0x00, 0x45, 0x00, 0x52, 0x00, 0x86, 0xc3,
	       0x50, 0x97, 0xac, 0x9c, 0xec, 0x10, 0x25, 0x54, 0x76, 0x4a,
	       0x57, 0xcc, 0xcc, 0x19, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	       0xaa, 0xaa, 0x68, 0xcd, 0x0a, 0xb8, 0x51, 0xe5, 0x1c, 0x96,
	       0xaa, 0xbc, 0x92, 0x7b, 0xeb, 0xef, 0x6a, 0x1c, 0x01, 0x01,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	       0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x0c, 0x00,
	       0x44, 0x00, 0x6f, 0x00, 0x6d, 0x00, 0x61, 0x00, 0x69, 0x00,
	       0x6e, 0x00, 0x01, 0x00, 0x0c, 0x00, 0x53, 0x00, 0x65, 0x00,
	       0x72, 0x00, 0x76, 0x00, 0x65, 0x00, 0x72, 0x00, 0x00, 0x00,
	       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc5, 0xda, 0xd2, 0x54,
	       0x4f, 0xc9, 0x79, 0x90, 0x94, 0xce, 0x1c, 0xe9, 0x0b, 0xc9,
	       0xd0, 0x3e ) );

/**
 * Report key generation test result
 *
 * @v test		Key generation test
 * @v file		Test code file
 * @v line		Test code line
 */
static void ntlm_key_okx ( struct ntlm_key_test *test,
			   const char *file, unsigned int line ) {
	struct ntlm_key key;

	ntlm_key ( test->domain, test->username, test->password, &key );
	okx ( memcmp ( &key, &test->expected, sizeof ( key ) ) == 0,
	      file, line );
}
#define ntlm_key_ok( test )						\
	ntlm_key_okx ( test, __FILE__, __LINE__ )

/**
 * Report NTLM variable-length data test result
 *
 * @v msg		Message header
 * @v msg_len		Length of message
 * @v data		Variable-length data descriptor
 * @v expected		Expected message header
 * @v expected_data	Expected variable-length data descriptor
 * @v field		Field name
 * @v file		Test code file
 * @v line		Test code line
 */
static void ntlm_data_okx ( struct ntlm_header *msg, size_t msg_len,
			    struct ntlm_data *data,
			    struct ntlm_header *expected,
			    struct ntlm_data *expected_data,
			    const char *field, const char *file,
			    unsigned int line ) {
	size_t offset;
	size_t len;
	void *raw;
	void *expected_raw;

	/* Verify data lies within message */
	okx ( data->len == data->max_len, file, line );
	offset = le32_to_cpu ( data->offset );
	len = le16_to_cpu ( data->len );
	okx ( offset <= msg_len, file, line );
	okx ( len <= ( msg_len - offset ), file, line );

	/* Verify content matches expected content */
	raw = ( ( ( void * ) msg ) + offset );
	expected_raw = ( ( ( void * ) expected ) +
			 le32_to_cpu ( expected_data->offset ) );
	DBGC ( msg, "NTLM %s expected:\n", field );
	DBGC_HDA ( msg, 0, expected_raw, le16_to_cpu ( expected_data->len ) );
	DBGC ( msg, "NTLM %s actual:\n", field );
	DBGC_HDA ( msg, 0, raw, len );
	okx ( data->len == expected_data->len, file, line );
	okx ( memcmp ( raw, expected_raw, len ) == 0, file, line );
}
#define ntlm_data_ok( msg, msg_len, data, expected, expected_data )	\
	ntlm_data_okx ( msg, msg_len, data, expected, expected_data,	\
			__FILE__, __LINE__ )

/**
 * Report NTLM authentication test result
 *
 * @v test		Authentication test
 * @v file		Test code file
 * @v line		Test code line
 */
static void ntlm_authenticate_okx ( struct ntlm_authenticate_test *test,
				    const char *file, unsigned int line ) {
	struct ntlm_authenticate *expected = test->expected;
	struct ntlm_challenge_info info;
	struct ntlm_authenticate *auth;
	struct ntlm_key key;
	struct ntlm_lm_response lm;
	struct ntlm_nt_response nt;
	size_t len;

	/* Parse Challenge message */
	okx ( ntlm_challenge ( test->challenge, test->challenge_len,
			       &info ) == 0, file, line );

	/* Generate key */
	ntlm_key ( test->domain, test->username, test->password, &key );

	/* Generate responses */
	ntlm_response ( &info, &key, &test->nonce, &lm, &nt );

	/* Allocate buffer for Authenticate message */
	len = ntlm_authenticate_len ( &info, test->domain, test->username,
				      test->workstation );
	okx ( len >= sizeof ( *auth ), file, line );
	auth = malloc ( len );
	okx ( auth != NULL, file, line );

	/* Construct Authenticate message */
	okx ( ntlm_authenticate ( &info, test->domain, test->username,
				  test->workstation, &lm, &nt, auth ) == len,
	      file, line );

	/* Verify header */
	okx ( memcmp ( &auth->header, &expected->header,
		       sizeof ( auth->header ) ) == 0, file, line );

	/* Verify LAN Manager response */
	ntlm_data_okx ( &auth->header, len, &auth->lm, &expected->header,
			&expected->lm, "LM", file, line );

	/* Verify NT response */
	ntlm_data_okx ( &auth->header, len, &auth->nt, &expected->header,
			&expected->nt, "NT", file, line );

	/* Verify domain name */
	ntlm_data_okx ( &auth->header, len, &auth->domain, &expected->header,
			&expected->domain, "domain", file, line );

	/* Verify user name */
	ntlm_data_okx ( &auth->header, len, &auth->user, &expected->header,
			&expected->user, "user", file, line );

	/* Verify workstation name */
	ntlm_data_okx ( &auth->header, len, &auth->workstation,
			&expected->header, &expected->workstation,
			"workstation",file, line );

	/* Verify session key */
	if ( auth->flags & NTLM_NEGOTIATE_KEY_EXCH ) {
		ntlm_data_okx ( &auth->header, len, &auth->session,
				&expected->header, &expected->session,
				"session", file, line );
	}

	/* Free Authenticate message */
	free ( auth );
}
#define ntlm_authenticate_ok( test )					\
	ntlm_authenticate_okx ( test, __FILE__, __LINE__ )

/**
 * Perform NTLM self-test
 *
 */
static void ntlm_test_exec ( void ) {

	/* Verify key generation */
	ntlm_key_ok ( &msnlmp_ntowfv2 );

	/* Verify authentication response */
	ntlm_authenticate_ok ( &msnlmp_authenticate );
}

/** NTLM self-test */
struct self_test ntlm_test __self_test = {
	.name = "ntlm",
	.exec = ntlm_test_exec,
};
