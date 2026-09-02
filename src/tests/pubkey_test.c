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
 * Public key self-tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ipxe/crypto.h>
#include <ipxe/test.h>
#include "pubkey_test.h"

/** Random data input */
static const uint8_t *pubkey_random;

/** Length of random data input */
static size_t pubkey_random_len;

/**
 * Get random data input
 *
 * @v data		Output buffer
 * @v len		Length of output buffer
 * @ret rc		Return status code
 */
int pubkey_test_get_random ( void *data, size_t len ) {

	/* Sanity check */
	assert ( len <= pubkey_random_len );

	/* Return random data */
	memcpy ( data, pubkey_random, len );

	/* Consume random data */
	pubkey_random += len;
	pubkey_random_len -= len;

	return 0;
}

/**
 * Report a public key encryption test result
 *
 * @v test		Public key encryption/decryption test
 * @v file		Test code file
 * @v line		Test code line
 */
void pubkey_encrypt_okx ( struct pubkey_encryption_test *test,
			  const char *file, unsigned int line ) {
	struct pubkey_algorithm *pubkey = test->key->pubkey;
	struct asn1_builder ciphertext;

	/* Sanity checks */
	okx ( test->key->public.len != 0, file, line );
	okx ( test->ciphertext.len != 0, file, line );

	/* Test encrypting with public key to obtain known ciphertext */
	ciphertext.data = NULL;
	ciphertext.len = 0;
	pubkey_random = test->random;
	pubkey_random_len = test->random_len;
	okx ( pubkey_encrypt ( pubkey, &test->key->public, &test->plaintext,
			       &ciphertext ) == 0, file, line );
	okx ( pubkey_random_len == 0, file, line );
	okx ( asn1_compare ( asn1_built ( &ciphertext ),
			     &test->ciphertext ) == 0, file, line );
	free ( ciphertext.data );
}

/**
 * Report a public key decryption test result
 *
 * @v test		Public key encryption/decryption test
 * @v file		Test code file
 * @v line		Test code line
 */
void pubkey_decrypt_okx ( struct pubkey_encryption_test *test,
			  const char *file, unsigned int line ) {
	struct pubkey_algorithm *pubkey = test->key->pubkey;
	struct asn1_builder plaintext;

	/* Sanity checks */
	okx ( test->key->private.len != 0, file, line );
	okx ( test->ciphertext.len != 0, file, line );

	/* Test decrypting with private key to obtain known plaintext */
	plaintext.data = NULL;
	plaintext.len = 0;
	okx ( pubkey_decrypt ( pubkey, &test->key->private, &test->ciphertext,
			       &plaintext ) == 0, file, line );
	okx ( asn1_compare ( asn1_built ( &plaintext ),
			     &test->plaintext ) == 0, file, line );
	free ( plaintext.data );
}

/**
 * Report a public key encryption failure test result
 *
 * @v test		Public key encryption/decryption test
 * @v file		Test code file
 * @v line		Test code line
 */
void pubkey_encrypt_fail_okx ( struct pubkey_encryption_test *test,
			       const char *file, unsigned int line ) {
	struct pubkey_algorithm *pubkey = test->key->pubkey;
	struct asn1_builder ciphertext;

	/* Test encrypting with public key */
	ciphertext.data = NULL;
	ciphertext.len = 0;
	pubkey_random = test->random;
	pubkey_random_len = test->random_len;
	okx ( pubkey_encrypt ( pubkey, &test->key->public, &test->plaintext,
			       &ciphertext ) != 0, file, line );
	free ( ciphertext.data );
}

/**
 * Report a public key decryption failure test result
 *
 * @v test		Public key encryption/decryption test
 * @v file		Test code file
 * @v line		Test code line
 */
void pubkey_decrypt_fail_okx ( struct pubkey_encryption_test *test,
			       const char *file, unsigned int line ) {
	struct pubkey_algorithm *pubkey = test->key->pubkey;
	struct asn1_builder plaintext;

	/* Test decrypting with private key to obtain known plaintext */
	plaintext.data = NULL;
	plaintext.len = 0;
	okx ( pubkey_decrypt ( pubkey, &test->key->private, &test->ciphertext,
			       &plaintext ) != 0, file, line );
	free ( plaintext.data );
}

/**
 * Report a public key encryption and decryption test result
 *
 * @v test		Public key encryption/decryption test
 * @v file		Test code file
 * @v line		Test code line
 */
void pubkey_encrypt_decrypt_okx ( struct pubkey_encryption_test *test,
				  const char *file, unsigned int line ) {
	struct pubkey_algorithm *pubkey = test->key->pubkey;
	struct asn1_builder plaintext;
	struct asn1_builder ciphertext;

	/* Sanity checks */
	okx ( test->key->private.len != 0, file, line );
	okx ( test->key->public.len != 0, file, line );
	okx ( test->ciphertext.len != 0, file, line );

	/* Test key matching */
	okx ( pubkey_match ( pubkey, &test->key->private,
			     &test->key->public ) == 0, file, line );

	/* Test encrypting with public key to obtain known ciphertext */
	pubkey_encrypt_okx ( test, file, line );

	/* Test decrypting with private key to obtain known plaintext */
	pubkey_decrypt_okx ( test, file, line );

	/* Test encrypting with private key and decrypting with public key */
	ciphertext.data = NULL;
	ciphertext.len = 0;
	plaintext.data = NULL;
	plaintext.len = 0;
	pubkey_random = test->random;
	pubkey_random_len = test->random_len;
	okx ( pubkey_encrypt ( pubkey, &test->key->private, &test->plaintext,
			       &ciphertext ) == 0, file, line );
	okx ( pubkey_random_len == 0, file, line );
	okx ( pubkey_decrypt ( pubkey, &test->key->public,
			       asn1_built ( &ciphertext ),
			       &plaintext ) == 0, file, line );
	okx ( asn1_compare ( asn1_built ( &plaintext ),
			     &test->plaintext ) == 0, file, line );
	free ( ciphertext.data );
	free ( plaintext.data );

	/* Test encrypting with public key and decrypting with private key */
	ciphertext.data = NULL;
	ciphertext.len = 0;
	plaintext.data = NULL;
	plaintext.len = 0;
	pubkey_random = test->random;
	pubkey_random_len = test->random_len;
	okx ( pubkey_encrypt ( pubkey, &test->key->public, &test->plaintext,
			       &ciphertext ) == 0, file, line );
	okx ( pubkey_random_len == 0, file, line );
	okx ( pubkey_decrypt ( pubkey, &test->key->private,
			       asn1_built ( &ciphertext ),
			       &plaintext ) == 0, file, line );
	okx ( asn1_compare ( asn1_built ( &plaintext ),
			     &test->plaintext ) == 0, file, line );
	free ( ciphertext.data );
	free ( plaintext.data );
}

/**
 * Report a public key signature test result
 *
 * @v test		Public key signature/verification test
 * @v file		Test code file
 * @v line		Test code line
 */
void pubkey_sign_okx ( struct pubkey_signature_test *test,
		       const char *file, unsigned int line ) {
	struct pubkey_algorithm *pubkey = test->key->pubkey;
	struct digest_algorithm *digest = test->digest;
	uint8_t digestctx[digest->ctxsize];
	uint8_t digestout[digest->digestsize];
	struct asn1_builder builder = { NULL, 0 };

	/* Sanity checks */
	okx ( test->key->private.len != 0, file, line );
	okx ( test->key->public.len != 0, file, line );
	okx ( test->signature.len != 0, file, line );

	/* Test key matching */
	okx ( pubkey_match ( pubkey, &test->key->private,
			     &test->key->public ) == 0, file, line );

	/* Construct digest over plaintext */
	digest_init ( digest, digestctx );
	digest_update ( digest, digestctx, test->plaintext,
			test->plaintext_len );
	digest_final ( digest, digestctx, digestout );

	/* Test signing using private key */
	pubkey_random = test->random;
	pubkey_random_len = test->random_len;
	okx ( pubkey_sign ( pubkey, &test->key->private, digest, digestout,
			    &builder ) == 0, file, line );
	okx ( pubkey_random_len == 0, file, line );
	okx ( builder.len != 0, file, line );
	okx ( asn1_compare ( asn1_built ( &builder ), &test->signature ) == 0,
	      file, line );

	/* Test verification of constructed signature */
	okx ( pubkey_verify ( pubkey, &test->key->public, digest, digestout,
			      asn1_built ( &builder ) ) == 0, file, line );

	/* Free signature */
	free ( builder.data );
}

/**
 * Report a public key verification test result
 *
 * @v test		Public key signature/verification test
 * @v file		Test code file
 * @v line		Test code line
 */
void pubkey_verify_okx ( struct pubkey_signature_test *test,
			 const char *file, unsigned int line ) {
	struct pubkey_algorithm *pubkey = test->key->pubkey;
	struct digest_algorithm *digest = test->digest;
	uint8_t digestctx[digest->ctxsize];
	uint8_t digestout[digest->digestsize];
	uint8_t signature[test->signature.len];
	struct asn1_cursor cursor = { signature, sizeof ( signature ) };
	uint8_t *bad;

	/* Sanity checks */
	okx ( test->key->public.len != 0, file, line );
	okx ( test->signature.len != 0, file, line );

	/* Construct digest over plaintext */
	digest_init ( digest, digestctx );
	digest_update ( digest, digestctx, test->plaintext,
			test->plaintext_len );
	digest_final ( digest, digestctx, digestout );

	/* Test verification using public key */
	okx ( pubkey_verify ( pubkey, &test->key->public, digest, digestout,
			      &test->signature ) == 0, file, line );

	/* Test verification failure of modified signature */
	memcpy ( signature, test->signature.data, sizeof ( signature ) );
	bad = ( signature + ( sizeof ( signature ) / 2 ) );
	*bad ^= 0x40;
	okx ( pubkey_verify ( pubkey, &test->key->public, digest, digestout,
			      &cursor ) != 0, file, line );
	*bad ^= 0x40;
	okx ( pubkey_verify ( pubkey, &test->key->public, digest, digestout,
			      &cursor ) == 0, file, line );
}

/**
 * Report a public key verification failure test result
 *
 * @v test		Public key signature/verification test
 * @v file		Test code file
 * @v line		Test code line
 */
void pubkey_verify_fail_okx ( struct pubkey_signature_test *test,
			      const char *file, unsigned int line ) {
	struct pubkey_algorithm *pubkey = test->key->pubkey;
	struct digest_algorithm *digest = test->digest;
	uint8_t digestctx[digest->ctxsize];
	uint8_t digestout[digest->digestsize];

	/* Construct digest over plaintext */
	digest_init ( digest, digestctx );
	digest_update ( digest, digestctx, test->plaintext,
			test->plaintext_len );
	digest_final ( digest, digestctx, digestout );

	/* Test verification using public key */
	okx ( pubkey_verify ( pubkey, &test->key->public, digest, digestout,
			      &test->signature ) != 0, file, line );
}

/**
 * Report a public key signature and verification test result
 *
 * @v test		Public key signature/verification test
 * @v file		Test code file
 * @v line		Test code line
 */
void pubkey_sign_verify_okx ( struct pubkey_signature_test *test,
			      const char *file, unsigned int line ) {
	struct pubkey_algorithm *pubkey = test->key->pubkey;

	/* Sanity checks */
	okx ( test->key->private.len != 0, file, line );
	okx ( test->key->public.len != 0, file, line );
	okx ( test->signature.len != 0, file, line );

	/* Test key matching */
	okx ( pubkey_match ( pubkey, &test->key->private,
			     &test->key->public ) == 0, file, line );

	/* Test signature */
	pubkey_sign_okx ( test, file, line );

	/* Test verification */
	pubkey_verify_okx ( test, file, line );
}
