#ifndef _HKDF_TEST_H
#define _HKDF_TEST_H

/** @file
 *
 * HKDF self-tests
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/crypto.h>
#include <ipxe/test.h>

/** Define inline input keying material */
#define IKM(...) { __VA_ARGS__ }

/** Define inline salt */
#define SALT(...) { __VA_ARGS__ }

/** Define inline additional information */
#define INFO(...) { __VA_ARGS__ }

/** Define inline expected pseudorandom key */
#define PRK(...) { __VA_ARGS__ }

/** Define inline absence of expected pseudorandom key */
#define PRK_UNSPECIFIED PRK()

/** Define inline expected output keying material */
#define OKM(...) { __VA_ARGS__ }

/** An HKDF self-test */
struct hkdf_test {
	/** Digest algorithm */
	struct digest_algorithm *digest;
	/** Input keying material */
	const void *ikm;
	/** Length of input keying material */
	size_t ikm_len;
	/** Salt */
	const void *salt;
	/** Length of salt */
	size_t salt_len;
	/** Additional information */
	const void *info;
	/** Length of additional information */
	size_t info_len;
	/** Expected pseudorandom key */
	const void *prk;
	/** Length of expected pseudorandom key */
	size_t prk_len;
	/** Expected output keying material */
	const void *okm;
	/** Length of expected output keying material */
	size_t okm_len;
};

/**
 * Define an HKDF test
 *
 * @v name		Test name
 * @v DIGEST		Digest algorithm
 * @v SALTED		Use salt value
 * @v IKM		Input keying material
 * @v SALT		Salt
 * @v INFO		Additional information
 * @v PRK		Expected pseudorandom key
 * @v OKM		Expected output keying material
 */
#define HKDF_TEST( name, DIGEST, SALTED, IKM, SALT, INFO, PRK, OKM )	\
	static const uint8_t name ## _ikm[] = IKM;			\
	static const uint8_t name ## _salt[] = SALT;			\
	static const uint8_t name ## _info[] = INFO;			\
	static const uint8_t name ## _prk[] = PRK;			\
	static const uint8_t name ## _okm[] = OKM;			\
	static struct hkdf_test name = {				\
		.digest = DIGEST,					\
		.ikm = name ## _ikm,					\
		.ikm_len = sizeof ( name ## _ikm ),			\
		.salt = ( SALTED ? name ## _salt : NULL ),		\
		.salt_len = sizeof ( name ## _salt ),			\
		.info = name ## _info,					\
		.info_len = sizeof ( name ## _info ),			\
		.prk = name ## _prk,					\
		.prk_len = sizeof ( name ## _prk ),			\
		.okm = name ## _okm,					\
		.okm_len = sizeof ( name ## _okm ),			\
	}

extern void hkdf_okx ( struct hkdf_test *test, const char *file,
		       unsigned int line );

/**
 * Report an HKDF test result
 *
 * @v test		HMAC test
 */
#define hkdf_ok( test ) hkdf_okx ( test, __FILE__, __LINE__ )

#endif /* _HKDF_TEST_H */
