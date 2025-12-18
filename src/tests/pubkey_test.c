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

/**
 * Report public key encryption and decryption test result
 *
 * @v test		Public key encryption and decryption test
 * @v file		Test code file
 * @v line		Test code line
 */
void pubkey_okx ( struct pubkey_test *test, const char *file,
		  unsigned int line ) {
	struct pubkey_algorithm *pubkey = test->pubkey;
	struct asn1_builder plaintext;
	struct asn1_builder ciphertext;

	/* Test key matching */
	okx ( pubkey_match ( pubkey, &test->private, &test->public ) == 0,
	      file, line );

	/* Test decrypting with private key to obtain known plaintext */
	plaintext.data = NULL;
	plaintext.len = 0;
	okx ( pubkey_decrypt ( pubkey, &test->private, &test->ciphertext,
			       &plaintext ) == 0, file, line );
	okx ( asn1_compare ( asn1_built ( &plaintext ),
			     &test->plaintext ) == 0, file, line );
	free ( plaintext.data );

	/* Test encrypting with private key and decrypting with public key */
	ciphertext.data = NULL;
	ciphertext.len = 0;
	plaintext.data = NULL;
	plaintext.len = 0;
	okx ( pubkey_encrypt ( pubkey, &test->private, &test->plaintext,
			       &ciphertext ) == 0, file, line );
	okx ( pubkey_decrypt ( pubkey, &test->public,
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
	okx ( pubkey_encrypt ( pubkey, &test->public, &test->plaintext,
			       &ciphertext ) == 0, file, line );
	okx ( pubkey_decrypt ( pubkey, &test->private,
			       asn1_built ( &ciphertext ),
			       &plaintext ) == 0, file, line );
	okx ( asn1_compare ( asn1_built ( &plaintext ),
			     &test->plaintext ) == 0, file, line );
	free ( ciphertext.data );
	free ( plaintext.data );
}

/**
 * Report public key signature test result
 *
 * @v test		Public key signature test
 * @v file		Test code file
 * @v line		Test code line
 */
void pubkey_sign_okx ( struct pubkey_sign_test *test, const char *file,
		       unsigned int line ) {
	struct pubkey_algorithm *pubkey = test->pubkey;
	struct digest_algorithm *digest = test->digest;
	uint8_t digestctx[digest->ctxsize];
	uint8_t digestout[digest->digestsize];
	uint8_t signature[test->signature.len];
	struct asn1_cursor cursor = { signature, sizeof ( signature ) };
	struct asn1_builder builder = { NULL, 0 };
	uint8_t *bad;

	/* Test key matching */
	okx ( pubkey_match ( pubkey, &test->private, &test->public ) == 0,
	      file, line );

	/* Construct digest over plaintext */
	digest_init ( digest, digestctx );
	digest_update ( digest, digestctx, test->plaintext,
			test->plaintext_len );
	digest_final ( digest, digestctx, digestout );

	/* Test verification using public key */
	okx ( pubkey_verify ( pubkey, &test->public, digest, digestout,
			      &test->signature ) == 0, file, line );

	/* Test verification failure of modified signature */
	memcpy ( signature, test->signature.data, sizeof ( signature ) );
	bad = ( signature + ( sizeof ( signature ) / 2 ) );
	*bad ^= 0x40;
	okx ( pubkey_verify ( pubkey, &test->public, digest, digestout,
			      &cursor ) != 0, file, line );
	*bad ^= 0x40;
	okx ( pubkey_verify ( pubkey, &test->public, digest, digestout,
			      &cursor ) == 0, file, line );

	/* Test signing using private key */
	okx ( pubkey_sign ( pubkey, &test->private, digest, digestout,
			    &builder ) == 0, file, line );
	okx ( builder.len != 0, file, line );
	okx ( asn1_compare ( asn1_built ( &builder ), &test->signature ) == 0,
	      file, line );

	/* Test verification of constructed signature */
	okx ( pubkey_verify ( pubkey, &test->public, digest, digestout,
			      asn1_built ( &builder ) ) == 0, file, line );

	/* Free signature */
	free ( builder.data );
}
