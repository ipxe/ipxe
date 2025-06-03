#ifndef _CIPHER_TEST_H
#define _CIPHER_TEST_H

/** @file
 *
 * Cipher self-tests
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/crypto.h>
#include <ipxe/test.h>

/** A cipher test */
struct cipher_test {
	/** Cipher algorithm */
	struct cipher_algorithm *cipher;
	/** Key */
	const void *key;
	/** Length of key */
	size_t key_len;
	/** Initialisation vector */
	const void *iv;
	/** Length of initialisation vector */
	size_t iv_len;
	/** Additional data */
	const void *additional;
	/** Length of additional data */
	size_t additional_len;
	/** Plaintext */
	const void *plaintext;
	/** Ciphertext */
	const void *ciphertext;
	/** Length of text */
	size_t len;
	/** Authentication tag */
	const void *auth;
	/** Length of authentication tag */
	size_t auth_len;
};

/** Define inline key */
#define KEY(...) { __VA_ARGS__ }

/** Define inline initialisation vector */
#define IV(...) { __VA_ARGS__ }

/** Define inline additional data */
#define ADDITIONAL(...) { __VA_ARGS__ }

/** Define inline plaintext data */
#define PLAINTEXT(...) { __VA_ARGS__ }

/** Define inline ciphertext data */
#define CIPHERTEXT(...) { __VA_ARGS__ }

/** Define inline authentication tag */
#define AUTH(...) { __VA_ARGS__ }

/**
 * Define a cipher test
 *
 * @v name		Test name
 * @v CIPHER		Cipher algorithm
 * @v KEY		Key
 * @v IV		Initialisation vector
 * @v ADDITIONAL	Additional data
 * @v PLAINTEXT		Plaintext
 * @v CIPHERTEXT	Ciphertext
 * @v AUTH		Authentication tag
 * @ret test		Cipher test
 */
#define CIPHER_TEST( name, CIPHER, KEY, IV, ADDITIONAL, PLAINTEXT,	\
		     CIPHERTEXT, AUTH )					\
	static const uint8_t name ## _key [] = KEY;			\
	static const uint8_t name ## _iv [] = IV;			\
	static const uint8_t name ## _additional [] = ADDITIONAL;	\
	static const uint8_t name ## _plaintext [] = PLAINTEXT;		\
	static const uint8_t name ## _ciphertext			\
		[ sizeof ( name ## _plaintext ) ] = CIPHERTEXT;		\
	static const uint8_t name ## _auth [] = AUTH;			\
	static struct cipher_test name = {				\
		.cipher = CIPHER,					\
		.key = name ## _key,					\
		.key_len = sizeof ( name ## _key ),			\
		.iv = name ## _iv,					\
		.iv_len = sizeof ( name ## _iv ),			\
		.additional = name ## _additional,			\
		.additional_len = sizeof ( name ## _additional ),	\
		.plaintext = name ## _plaintext,			\
		.ciphertext = name ## _ciphertext,			\
		.len = sizeof ( name ## _plaintext ),			\
		.auth = name ## _auth,					\
		.auth_len = sizeof ( name ## _auth ),			\
	}

extern void cipher_encrypt_okx ( struct cipher_test *test, const char *file,
				 unsigned int line );
extern void cipher_decrypt_okx ( struct cipher_test *test, const char *file,
				 unsigned int line );
extern void cipher_okx ( struct cipher_test *test, const char *file,
			 unsigned int line );
extern unsigned long cipher_cost_encrypt ( struct cipher_algorithm *cipher,
					   size_t key_len );
extern unsigned long cipher_cost_decrypt ( struct cipher_algorithm *cipher,
					   size_t key_len );

/**
 * Report a cipher encryption test result
 *
 * @v test		Cipher test
 */
#define cipher_encrypt_ok( test ) \
	cipher_encrypt_okx ( test, __FILE__, __LINE__ )

/**
 * Report a cipher decryption test result
 *
 * @v test		Cipher test
 */
#define cipher_decrypt_ok( test ) \
	cipher_decrypt_okx ( test, __FILE__, __LINE__ )

/**
 * Report a cipher encryption and decryption test result
 *
 * @v test		Cipher test
 */
#define cipher_ok( test ) \
	cipher_okx ( test, __FILE__, __LINE__ )

#endif /* _CIPHER_TEST_H */
