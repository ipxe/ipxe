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
 * HMAC self-tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <string.h>
#include <ipxe/hmac.h>
#include <ipxe/md5.h>
#include <ipxe/sha1.h>
#include <ipxe/sha256.h>
#include <ipxe/test.h>

/** Define inline key data */
#define KEY(...) { __VA_ARGS__ }

/** Define inline data */
#define DATA(...) { __VA_ARGS__ }

/** Define inline expected HMAC */
#define EXPECTED(...) { __VA_ARGS__ }

/** An HMAC test */
struct hmac_test {
	/** Digest algorithm */
	struct digest_algorithm *digest;
	/** Key */
	const void *key;
	/** Length of key */
	size_t key_len;
	/** Data */
	const void *data;
	/** Length of data */
	size_t data_len;
	/** Expected HMAC */
	const void *expected;
	/** Length of expected HMAC */
	size_t expected_len;
};

/**
 * Define an HMAC test
 *
 * @v name		Test name
 * @v DIGEST		Digest algorithm
 * @v KEY		Key
 * @v DATA		Data
 * @v EXPECTED		Expected HMAC
 * @ret test		HMAC test
 */
#define HMAC_TEST( name, DIGEST, KEY, DATA, EXPECTED )			\
	static const uint8_t name ## _key[] = KEY;			\
	static const uint8_t name ## _data[] = DATA;			\
	static const uint8_t name ## _expected[] = EXPECTED;		\
	static struct hmac_test name = {				\
		.digest = DIGEST,					\
		.key = name ## _key,					\
		.key_len = sizeof ( name ## _key ),			\
		.data = name ## _data,					\
		.data_len = sizeof ( name ## _data ),			\
		.expected = name ## _expected,				\
		.expected_len = sizeof ( name ## _expected ),		\
	}

/**
 * Report an HMAC test result
 *
 * @v test		HMAC test
 * @v file		Test code file
 * @v line		Test code line
 */
static void hmac_okx ( struct hmac_test *test, const char *file,
		       unsigned int line ) {
	struct digest_algorithm *digest = test->digest;
	uint8_t ctx[ hmac_ctxsize ( digest ) ];
	uint8_t hmac[digest->digestsize];

	/* Sanity checks */
	okx ( sizeof ( ctx ) == ( digest->ctxsize + digest->blocksize ),
	      file, line );
	okx ( test->expected_len == digest->digestsize, file, line );

	/* Calculate HMAC */
	DBGC ( test, "HMAC-%s key:\n", digest->name );
	DBGC_HDA ( test, 0, test->key, test->key_len );
	DBGC ( test, "HMAC-%s data:\n", digest->name );
	DBGC_HDA ( test, 0, test->data, test->data_len );
	hmac_init ( digest, ctx, test->key, test->key_len );
	hmac_update ( digest, ctx, test->data, test->data_len );
	hmac_final ( digest, ctx, hmac );
	DBGC ( test, "HMAC-%s result:\n", digest->name );
	DBGC_HDA ( test, 0, hmac, sizeof ( hmac ) );

	/* Compare against expected result */
	okx ( memcmp ( hmac, test->expected, test->expected_len ) == 0,
	      file, line );
}
#define hmac_ok( test ) hmac_okx ( test, __FILE__, __LINE__ )

/* Empty key and data */
HMAC_TEST ( hmac_empty, &sha256_algorithm, KEY(), DATA(),
	    EXPECTED ( 0xb6, 0x13, 0x67, 0x9a, 0x08, 0x14, 0xd9, 0xec, 0x77,
		       0x2f, 0x95, 0xd7, 0x78, 0xc3, 0x5f, 0xc5, 0xff, 0x16,
		       0x97, 0xc4, 0x93, 0x71, 0x56, 0x53, 0xc6, 0xc7, 0x12,
		       0x14, 0x42, 0x92, 0xc5, 0xad ) );

/* "Hello world" */
HMAC_TEST ( hmac_hw, &sha256_algorithm,
	    KEY ( 'H', 'e', 'l', 'l', 'o' ),
	    DATA ( 'W', 'o', 'r', 'l', 'd' ),
	    EXPECTED ( 0x59, 0x16, 0x8e, 0x30, 0x9f, 0x2c, 0x97, 0xdd, 0x04,
		       0xe4, 0x5b, 0xe3, 0xe7, 0x9b, 0xd9, 0xac, 0xb6, 0xd2,
		       0x2f, 0xda, 0x65, 0x46, 0xc0, 0x0c, 0x53, 0x92, 0x82,
		       0xc4, 0x1e, 0xeb, 0x91, 0x6e ) );

/* "Hello world" using SHA-1 */
HMAC_TEST ( hmac_hw_sha1, &sha1_algorithm,
	    KEY ( 'H', 'e', 'l', 'l', 'o' ),
	    DATA ( 'W', 'o', 'r', 'l', 'd' ),
	    EXPECTED ( 0x9e, 0x29, 0xcf, 0x6d, 0x48, 0x90, 0x49, 0x9e, 0xf8,
		       0x5a, 0x31, 0x47, 0x55, 0x7b, 0x1a, 0x45, 0xd5, 0xae,
		       0xd1, 0x77 ) );

/* "Hello world" using MD5 */
HMAC_TEST ( hmac_hw_md5, &md5_algorithm,
	    KEY ( 'H', 'e', 'l', 'l', 'o' ),
	    DATA ( 'W', 'o', 'r', 'l', 'd' ),
	    EXPECTED ( 0xc7, 0x1c, 0x0c, 0xd0, 0xdc, 0x24, 0x49, 0xbd, 0xd8,
		       0x9d, 0x28, 0xeb, 0x03, 0xbe, 0xf2, 0x04 ) );

/* Block-length key */
HMAC_TEST ( hmac_maxlen, &sha256_algorithm,
	    KEY ( 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
		  0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13,
		  0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
		  0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
		  0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31,
		  0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
		  0x3c, 0x3d, 0x3e, 0x3f ),
	    DATA ( 'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd' ),
	    EXPECTED ( 0xdd, 0x05, 0xcc, 0xe6, 0xd6, 0xaf, 0x91, 0x61, 0x4b,
		       0xaf, 0x35, 0x6b, 0x86, 0x0a, 0x05, 0x67, 0x25, 0x22,
		       0xf0, 0x54, 0xd2, 0x5f, 0xd7, 0xe1, 0x54, 0x26, 0x01,
		       0x16, 0xfd, 0x8a, 0xf3, 0x5b ) );

/* Over-length key */
HMAC_TEST ( hmac_overlen, &sha256_algorithm,
	    KEY ( 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
		  0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13,
		  0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
		  0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
		  0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31,
		  0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
		  0x3c, 0x3d, 0x3e, 0x3f, 0x40 ),
	    DATA ( 'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd' ),
	    EXPECTED ( 0x4f, 0x0f, 0x42, 0x07, 0xda, 0x84, 0x3d, 0x2a, 0x34,
		       0xae, 0x5d, 0xd9, 0x05, 0x1e, 0x96, 0xa8, 0xb6, 0xef,
		       0xa1, 0xcd, 0x49, 0x5b, 0xea, 0x30, 0xbf, 0x47, 0x3b,
		       0xdc, 0xa9, 0x86, 0xbb, 0x31 ) );

/**
 * Perform HMAC self-tests
 *
 */
static void hmac_test_exec ( void ) {

	hmac_ok ( &hmac_empty );
	hmac_ok ( &hmac_hw );
	hmac_ok ( &hmac_hw_sha1 );
	hmac_ok ( &hmac_hw_md5 );
	hmac_ok ( &hmac_maxlen );
	hmac_ok ( &hmac_overlen );
}

/** HMAC self-tests */
struct self_test hmac_test __self_test = {
	.name = "hmac",
	.exec = hmac_test_exec,
};
