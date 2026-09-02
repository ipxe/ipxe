#ifndef _PUBKEY_TEST_H
#define _PUBKEY_TEST_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/crypto.h>
#include <ipxe/test.h>

/** A public-key test */
struct pubkey_test {
	/** Public-key algorithm */
	struct pubkey_algorithm *pubkey;
	/** Private key */
	const struct asn1_cursor private;
	/** Public key */
	const struct asn1_cursor public;
};

/** A public-key encryption/decryption test */
struct pubkey_encryption_test {
	/** Test key */
	const struct pubkey_test *key;
	/** Random data input */
	const void *random;
	/** Random data input length */
	size_t random_len;
	/** Plaintext */
	const struct asn1_cursor plaintext;
	/** Ciphertext */
	const struct asn1_cursor ciphertext;
};

/** A public-key signature/verification test */
struct pubkey_signature_test {
	/** Test key */
	const struct pubkey_test *key;
	/** Random data input */
	const void *random;
	/** Random data input length */
	size_t random_len;
	/** Plaintext */
	const void *plaintext;
	/** Plaintext length */
	size_t plaintext_len;
	/** Signature algorithm */
	struct digest_algorithm *digest;
	/** Signature */
	const struct asn1_cursor signature;
};

/** Define inline private key data */
#define PRIVATE(...) { __VA_ARGS__ }

/** Define inline public key data */
#define PUBLIC(...) { __VA_ARGS__ }

/** Define inline random data */
#define RANDOM(...) { __VA_ARGS__ }

/** Define inline plaintext data */
#define PLAINTEXT(...) { __VA_ARGS__ }

/** Define inline ciphertext data */
#define CIPHERTEXT(...) { __VA_ARGS__ }

/** Define inline signature data */
#define SIGNATURE(...) { __VA_ARGS__ }

/**
 * Define a public-key test
 *
 * @v name		Test name
 * @v PUBKEY		Public-key algorithm
 * @v PRIVATE		Private key
 * @v PUBLIC		Public key
 * @v RANDOM		Random data
 * @v PLAINTEXT		Plaintext
 * @v CIPHERTEXT	Ciphertext
 * @ret test		Encryption and decryption test
 */
#define PUBKEY_TEST( name, PUBKEY, PRIVATE, PUBLIC )			\
	static const uint8_t name ## _private[] = PRIVATE;		\
	static const uint8_t name ## _public[] = PUBLIC;		\
	static struct pubkey_test name = {				\
		.pubkey = PUBKEY,					\
		.private = {						\
			.data = name ## _private,			\
			.len = sizeof ( name ## _private ),		\
		},							\
		.public = {						\
			.data = name ## _public,			\
			.len = sizeof ( name ## _public ),		\
		},							\
	}

/**
 * Define a public-key encryption/decryption test
 *
 * @v name		Test name
 * @v KEY		Test key
 * @v RANDOM		Random data
 * @v PLAINTEXT		Plaintext
 * @v CIPHERTEXT	Ciphertext
 * @ret test		Encryption and decryption test
 */
#define PUBKEY_ENCRYPTION_TEST( name, KEY, RANDOM, PLAINTEXT,		\
				CIPHERTEXT )				\
	static const uint8_t name ## _random[] = RANDOM;		\
	static const uint8_t name ## _plaintext[] = PLAINTEXT;		\
	static const uint8_t name ## _ciphertext[] = CIPHERTEXT;	\
	static struct pubkey_encryption_test name = {			\
		.key = KEY,						\
		.random = name ## _random,				\
		.random_len = sizeof ( name ## _random ),		\
		.plaintext = {						\
			.data = name ## _plaintext,			\
			.len = sizeof ( name ## _plaintext ),		\
		},							\
		.ciphertext = {						\
			.data = name ## _ciphertext,			\
			.len = sizeof ( name ## _ciphertext ),		\
		},							\
	}

/**
 * Define a public-key signature/verification test
 *
 * @v name		Test name
 * @v KEY		Test key
 * @v RANDOM		Random data
 * @v PLAINTEXT		Plaintext
 * @v DIGEST		Digest algorithm
 * @v SIGNATURE		Signature
 * @ret test		Signature test
 */
#define PUBKEY_SIGNATURE_TEST( name, KEY, RANDOM, PLAINTEXT, DIGEST,	\
			       SIGNATURE )				\
	static const uint8_t name ## _random[] = RANDOM;		\
	static const uint8_t name ## _plaintext[] = PLAINTEXT;		\
	static const uint8_t name ## _signature[] = SIGNATURE;		\
	static struct pubkey_signature_test name = {			\
		.key = KEY,						\
		.random = name ## _random,				\
		.random_len = sizeof ( name ## _random ),		\
		.plaintext = name ## _plaintext,			\
		.plaintext_len = sizeof ( name ## _plaintext ),		\
		.digest = DIGEST,					\
		.signature = {						\
			.data = name ## _signature,			\
			.len = sizeof ( name ## _signature ),		\
		},							\
	}

extern int pubkey_test_get_random ( void *data, size_t len );
extern void pubkey_encrypt_okx ( struct pubkey_encryption_test *test,
				 const char *file, unsigned int line );
extern void pubkey_decrypt_okx ( struct pubkey_encryption_test *test,
				 const char *file, unsigned int line );
extern void pubkey_encrypt_fail_okx ( struct pubkey_encryption_test *test,
				      const char *file, unsigned int line );
extern void pubkey_decrypt_fail_okx ( struct pubkey_encryption_test *test,
				      const char *file, unsigned int line );
extern void pubkey_encrypt_decrypt_okx ( struct pubkey_encryption_test *test,
					 const char *file,
					 unsigned int line );
extern void pubkey_sign_okx ( struct pubkey_signature_test *test,
			      const char *file, unsigned int line );
extern void pubkey_verify_okx ( struct pubkey_signature_test *test,
				const char *file, unsigned int line );
extern void pubkey_verify_fail_okx ( struct pubkey_signature_test *test,
				     const char *file, unsigned int line );
extern void pubkey_sign_verify_okx ( struct pubkey_signature_test *test,
				     const char *file, unsigned int line );

/**
 * Report a public key encryption test result
 *
 * @v test		Public key encryption/decryption test
 */
#define pubkey_encrypt_ok( test ) \
	pubkey_encrypt_okx ( test, __FILE__, __LINE__ )

/**
 * Report a public key decryption test result
 *
 * @v test		Public key encryption/decryption test
 */
#define pubkey_decrypt_ok( test ) \
	pubkey_decrypt_okx ( test, __FILE__, __LINE__ )

/**
 * Report a public key encryption failure test result
 *
 * @v test		Public key encryption/decryption test
 */
#define pubkey_encrypt_fail_ok( test ) \
	pubkey_encrypt_fail_okx ( test, __FILE__, __LINE__ )

/**
 * Report a public key decryption failure test result
 *
 * @v test		Public key decryption/decryption test
 */
#define pubkey_decrypt_fail_ok( test ) \
	pubkey_decrypt_fail_okx ( test, __FILE__, __LINE__ )

/**
 * Report a public key encryption and decryption test result
 *
 * @v test		Public key encryption/decryption test
 */
#define pubkey_encrypt_decrypt_ok( test ) \
	pubkey_encrypt_decrypt_okx ( test, __FILE__, __LINE__ )

/**
 * Report a public key signature test result
 *
 * @v test		Public key signature/verification test
 */
#define pubkey_sign_ok( test ) \
	pubkey_sign_okx ( test, __FILE__, __LINE__ )

/**
 * Report a public key verification test result
 *
 * @v test		Public key signature/verification test
 */
#define pubkey_verify_ok( test ) \
	pubkey_verify_okx ( test, __FILE__, __LINE__ )

/**
 * Report a public key verification failure test result
 *
 * @v test		Public key signature/verification test
 */
#define pubkey_verify_fail_ok( test ) \
	pubkey_verify_fail_okx ( test, __FILE__, __LINE__ )

/**
 * Report a public key signature and verification test result
 *
 * @v test		Public key signature/verification test
 */
#define pubkey_sign_verify_ok( test ) \
	pubkey_sign_verify_okx ( test, __FILE__, __LINE__ )

#endif /* _PUBKEY_TEST_H */
