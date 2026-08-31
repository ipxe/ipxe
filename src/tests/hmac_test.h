#ifndef _HMAC_TEST_H
#define _HMAC_TEST_H

/** @file
 *
 * HMAC self-tests
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/crypto.h>
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

extern void hmac_okx ( struct hmac_test *test, const char *file,
		       unsigned int line );

/**
 * Report an HMAC test result
 *
 * @v test		HMAC test
 */
#define hmac_ok( test ) hmac_okx ( test, __FILE__, __LINE__ )

#endif /* _HMAC_TEST_H */
