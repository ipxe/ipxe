#ifndef _PUBKEY_TEST_H
#define _PUBKEY_TEST_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/crypto.h>
#include <ipxe/test.h>

/** A public-key encryption and decryption test */
struct pubkey_test {
	/** Public-key algorithm */
	struct pubkey_algorithm *pubkey;
	/** Private key */
	const struct asn1_cursor private;
	/** Public key */
	const struct asn1_cursor public;
	/** Plaintext */
	const void *plaintext;
	/** Length of plaintext */
	size_t plaintext_len;
	/** Ciphertext
	 *
	 * Note that the encryption process may include some random
	 * padding, so a given plaintext will encrypt to multiple
	 * different ciphertexts.
	 */
	const void *ciphertext;
	/** Length of ciphertext */
	size_t ciphertext_len;
};

/** A public-key signature test */
struct pubkey_sign_test {
	/** Public-key algorithm */
	struct pubkey_algorithm *pubkey;
	/** Private key */
	const struct asn1_cursor private;
	/** Public key */
	const struct asn1_cursor public;
	/** Plaintext */
	const void *plaintext;
	/** Plaintext length */
	size_t plaintext_len;
	/** Signature algorithm */
	struct digest_algorithm *digest;
	/** Signature */
	const void *signature;
	/** Signature length */
	size_t signature_len;
};

/** Define inline private key data */
#define PRIVATE(...) { __VA_ARGS__ }

/** Define inline public key data */
#define PUBLIC(...) { __VA_ARGS__ }

/** Define inline plaintext data */
#define PLAINTEXT(...) { __VA_ARGS__ }

/** Define inline ciphertext data */
#define CIPHERTEXT(...) { __VA_ARGS__ }

/** Define inline signature data */
#define SIGNATURE(...) { __VA_ARGS__ }

/**
 * Define a public-key encryption and decryption test
 *
 * @v name		Test name
 * @v PUBKEY		Public-key algorithm
 * @v PRIVATE		Private key
 * @v PUBLIC		Public key
 * @v PLAINTEXT		Plaintext
 * @v CIPHERTEXT	Ciphertext
 * @ret test		Encryption and decryption test
 */
#define PUBKEY_TEST( name, PUBKEY, PRIVATE, PUBLIC, PLAINTEXT,		\
			   CIPHERTEXT )					\
	static const uint8_t name ## _private[] = PRIVATE;		\
	static const uint8_t name ## _public[] = PUBLIC;		\
	static const uint8_t name ## _plaintext[] = PLAINTEXT;		\
	static const uint8_t name ## _ciphertext[] = CIPHERTEXT;	\
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
		.plaintext = name ## _plaintext,			\
		.plaintext_len = sizeof ( name ## _plaintext ),		\
		.ciphertext = name ## _ciphertext,			\
		.ciphertext_len = sizeof ( name ## _ciphertext ),	\
	}

/**
 * Define a public-key signature test
 *
 * @v name		Test name
 * @v PUBKEY		Public-key algorithm
 * @v PRIVATE		Private key
 * @v PUBLIC		Public key
 * @v PLAINTEXT		Plaintext
 * @v DIGEST		Digest algorithm
 * @v SIGNATURE		Signature
 * @ret test		Signature test
 */
#define PUBKEY_SIGN_TEST( name, PUBKEY, PRIVATE, PUBLIC, PLAINTEXT,	\
			  DIGEST, SIGNATURE )				\
	static const uint8_t name ## _private[] = PRIVATE;		\
	static const uint8_t name ## _public[] = PUBLIC;		\
	static const uint8_t name ## _plaintext[] = PLAINTEXT;		\
	static const uint8_t name ## _signature[] = SIGNATURE;		\
	static struct pubkey_sign_test name = {				\
		.pubkey = PUBKEY,					\
		.private = {						\
			.data = name ## _private,			\
			.len = sizeof ( name ## _private ),		\
		},							\
		.public = {						\
			.data = name ## _public,			\
			.len = sizeof ( name ## _public ),		\
		},							\
		.plaintext = name ## _plaintext,			\
		.plaintext_len = sizeof ( name ## _plaintext ),		\
		.digest = DIGEST,					\
		.signature = name ## _signature,			\
		.signature_len = sizeof ( name ## _signature ),		\
	}

extern void pubkey_okx ( struct pubkey_test *test,
			 const char *file, unsigned int line );
extern void pubkey_sign_okx ( struct pubkey_sign_test *test,
			      const char *file, unsigned int line );

/**
 * Report a public key encryption and decryption test result
 *
 * @v test		Public key encryption and decryption test
 */
#define pubkey_ok( test ) \
	pubkey_okx ( test, __FILE__, __LINE__ )

/**
 * Report a public key signature test result
 *
 * @v test		Public key signature test
 */
#define pubkey_sign_ok( test ) \
	pubkey_sign_okx ( test, __FILE__, __LINE__ )

#endif /* _PUBKEY_TEST_H */
