/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
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
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ipxe/asn1.h>
#include <ipxe/crypto.h>
#include <ipxe/bigint.h>
#include <ipxe/random_nz.h>
#include <ipxe/rsa.h>

/** @file
 *
 * RSA public-key cryptography
 *
 * RSA is documented in RFC 3447.
 */

/* Disambiguate the various error causes */
#define EACCES_VERIFY \
	__einfo_error ( EINFO_EACCES_VERIFY )
#define EINFO_EACCES_VERIFY \
	__einfo_uniqify ( EINFO_EACCES, 0x01, "RSA signature incorrect" )

/** An RSA context */
struct rsa_context {
	/** Allocated memory */
	void *dynamic;
	/** Modulus */
	bigint_element_t *modulus0;
	/** Modulus size */
	unsigned int size;
	/** Modulus length */
	size_t max_len;
	/** Exponent */
	bigint_element_t *exponent0;
	/** Exponent size */
	unsigned int exponent_size;
	/** Input buffer */
	bigint_element_t *input0;
	/** Output buffer */
	bigint_element_t *output0;
	/** Temporary working space for modular exponentiation */
	void *tmp;
};

/**
 * Identify RSA prefix
 *
 * @v digest		Digest algorithm
 * @ret prefix		RSA prefix, or NULL
 */
static struct rsa_digestinfo_prefix *
rsa_find_prefix ( struct digest_algorithm *digest ) {
	struct rsa_digestinfo_prefix *prefix;

	for_each_table_entry ( prefix, RSA_DIGESTINFO_PREFIXES ) {
		if ( prefix->digest == digest )
			return prefix;
	}
	return NULL;
}

/**
 * Free RSA dynamic storage
 *
 * @v context		RSA context
 */
static inline void rsa_free ( struct rsa_context *context ) {

	free ( context->dynamic );
}

/**
 * Allocate RSA dynamic storage
 *
 * @v context		RSA context
 * @v modulus_len	Modulus length
 * @v exponent_len	Exponent length
 * @ret rc		Return status code
 */
static int rsa_alloc ( struct rsa_context *context, size_t modulus_len,
		       size_t exponent_len ) {
	unsigned int size = bigint_required_size ( modulus_len );
	unsigned int exponent_size = bigint_required_size ( exponent_len );
	bigint_t ( size ) *modulus;
	size_t tmp_len = bigint_mod_exp_tmp_len ( modulus );
	struct {
		bigint_t ( size ) modulus;
		bigint_t ( exponent_size ) exponent;
		bigint_t ( size ) input;
		bigint_t ( size ) output;
		uint8_t tmp[tmp_len];
	} __attribute__ (( packed )) *dynamic;

	/* Allocate dynamic storage */
	dynamic = malloc ( sizeof ( *dynamic ) );
	if ( ! dynamic )
		return -ENOMEM;

	/* Assign dynamic storage */
	context->dynamic = dynamic;
	context->modulus0 = &dynamic->modulus.element[0];
	context->size = size;
	context->max_len = modulus_len;
	context->exponent0 = &dynamic->exponent.element[0];
	context->exponent_size = exponent_size;
	context->input0 = &dynamic->input.element[0];
	context->output0 = &dynamic->output.element[0];
	context->tmp = &dynamic->tmp;

	return 0;
}

/**
 * Parse RSA modulus and exponent
 *
 * @v modulus		Modulus to fill in
 * @v exponent		Exponent to fill in
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int rsa_parse_mod_exp ( struct asn1_cursor *modulus,
			       struct asn1_cursor *exponent,
			       const struct asn1_cursor *raw ) {
	struct asn1_cursor cursor;
	int is_private;
	int rc;

	/* Enter subjectPublicKeyInfo/privateKeyInfo/RSAPrivateKey */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_SEQUENCE );

	/* Determine key format */
	if ( asn1_type ( &cursor ) == ASN1_INTEGER ) {

		/* Private key */
		is_private = 1;

		/* Skip version */
		asn1_skip_any ( &cursor );

		/* Enter privateKey, if present */
		if ( asn1_check_algorithm ( &cursor, &rsa_encryption_algorithm,
					    NULL ) == 0 ) {

			/* Skip privateKeyAlgorithm */
			asn1_skip_any ( &cursor );

			/* Enter privateKey */
			asn1_enter ( &cursor, ASN1_OCTET_STRING );

			/* Enter RSAPrivateKey */
			asn1_enter ( &cursor, ASN1_SEQUENCE );

			/* Skip version */
			asn1_skip ( &cursor, ASN1_INTEGER );
		}

	} else {

		/* Public key */
		is_private = 0;

		/* Skip algorithm */
		asn1_skip ( &cursor, ASN1_SEQUENCE );

		/* Enter subjectPublicKey */
		asn1_enter_bits ( &cursor, NULL );

		/* Enter RSAPublicKey */
		asn1_enter ( &cursor, ASN1_SEQUENCE );
	}

	/* Extract modulus */
	memcpy ( modulus, &cursor, sizeof ( *modulus ) );
	if ( ( rc = asn1_enter_unsigned ( modulus ) ) != 0 )
		return rc;
	asn1_skip_any ( &cursor );

	/* Skip public exponent, if applicable */
	if ( is_private )
		asn1_skip ( &cursor, ASN1_INTEGER );

	/* Extract publicExponent/privateExponent */
	memcpy ( exponent, &cursor, sizeof ( *exponent ) );
	if ( ( rc = asn1_enter_unsigned ( exponent ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Initialise RSA cipher
 *
 * @v context		RSA context
 * @v key		Key
 * @ret rc		Return status code
 */
static int rsa_init ( struct rsa_context *context,
		      const struct asn1_cursor *key ) {
	struct asn1_cursor modulus;
	struct asn1_cursor exponent;
	int rc;

	/* Initialise context */
	memset ( context, 0, sizeof ( *context ) );

	/* Parse modulus and exponent */
	if ( ( rc = rsa_parse_mod_exp ( &modulus, &exponent, key ) ) != 0 ){
		DBGC ( context, "RSA %p invalid modulus/exponent:\n", context );
		DBGC_HDA ( context, 0, key->data, key->len );
		goto err_parse;
	}

	DBGC ( context, "RSA %p modulus:\n", context );
	DBGC_HDA ( context, 0, modulus.data, modulus.len );
	DBGC ( context, "RSA %p exponent:\n", context );
	DBGC_HDA ( context, 0, exponent.data, exponent.len );

	/* Allocate dynamic storage */
	if ( ( rc = rsa_alloc ( context, modulus.len, exponent.len ) ) != 0 )
		goto err_alloc;

	/* Construct big integers */
	bigint_init ( ( ( bigint_t ( context->size ) * ) context->modulus0 ),
		      modulus.data, modulus.len );
	bigint_init ( ( ( bigint_t ( context->exponent_size ) * )
			context->exponent0 ), exponent.data, exponent.len );

	return 0;

	rsa_free ( context );
 err_alloc:
 err_parse:
	return rc;
}

/**
 * Perform RSA cipher operation
 *
 * @v context		RSA context
 * @v in		Input buffer
 * @v out		Output buffer
 */
static void rsa_cipher ( struct rsa_context *context,
			 const void *in, void *out ) {
	bigint_t ( context->size ) *input = ( ( void * ) context->input0 );
	bigint_t ( context->size ) *output = ( ( void * ) context->output0 );
	bigint_t ( context->size ) *modulus = ( ( void * ) context->modulus0 );
	bigint_t ( context->exponent_size ) *exponent =
		( ( void * ) context->exponent0 );

	/* Initialise big integer */
	bigint_init ( input, in, context->max_len );

	/* Perform modular exponentiation */
	bigint_mod_exp ( input, modulus, exponent, output, context->tmp );

	/* Copy out result */
	bigint_done ( output, out, context->max_len );
}

/**
 * Encrypt using RSA
 *
 * @v key		Key
 * @v plaintext		Plaintext
 * @v ciphertext	Ciphertext
 * @ret ciphertext_len	Length of ciphertext, or negative error
 */
static int rsa_encrypt ( const struct asn1_cursor *key,
			 const struct asn1_cursor *plaintext,
			 struct asn1_builder *ciphertext ) {
	struct rsa_context context;
	void *temp;
	uint8_t *encoded;
	size_t max_len;
	size_t random_nz_len;
	int rc;

	DBGC ( &context, "RSA %p encrypting:\n", &context );
	DBGC_HDA ( &context, 0, plaintext->data, plaintext->len );

	/* Initialise context */
	if ( ( rc = rsa_init ( &context, key ) ) != 0 )
		goto err_init;

	/* Calculate lengths */
	max_len = ( context.max_len - 11 );
	random_nz_len = ( max_len - plaintext->len + 8 );

	/* Sanity check */
	if ( plaintext->len > max_len ) {
		DBGC ( &context, "RSA %p plaintext too long (%zd bytes, max "
		       "%zd)\n", &context, plaintext->len, max_len );
		rc = -ERANGE;
		goto err_sanity;
	}

	/* Construct encoded message (using the big integer output
	 * buffer as temporary storage)
	 */
	temp = context.output0;
	encoded = temp;
	encoded[0] = 0x00;
	encoded[1] = 0x02;
	if ( ( rc = get_random_nz ( &encoded[2], random_nz_len ) ) != 0 ) {
		DBGC ( &context, "RSA %p could not generate random data: %s\n",
		       &context, strerror ( rc ) );
		goto err_random;
	}
	encoded[ 2 + random_nz_len ] = 0x00;
	memcpy ( &encoded[ context.max_len - plaintext->len ],
		 plaintext->data, plaintext->len );

	/* Create space for ciphertext */
	if ( ( rc = asn1_grow ( ciphertext, context.max_len ) ) != 0 )
		goto err_grow;

	/* Encipher the encoded message */
	rsa_cipher ( &context, encoded, ciphertext->data );
	DBGC ( &context, "RSA %p encrypted:\n", &context );
	DBGC_HDA ( &context, 0, ciphertext->data, context.max_len );

	/* Free context */
	rsa_free ( &context );

	return 0;

 err_grow:
 err_random:
 err_sanity:
	rsa_free ( &context );
 err_init:
	return rc;
}

/**
 * Decrypt using RSA
 *
 * @v key		Key
 * @v ciphertext	Ciphertext
 * @v plaintext		Plaintext
 * @ret rc		Return status code
 */
static int rsa_decrypt ( const struct asn1_cursor *key,
			 const struct asn1_cursor *ciphertext,
			 struct asn1_builder *plaintext ) {
	struct rsa_context context;
	void *temp;
	uint8_t *encoded;
	uint8_t *end;
	uint8_t *zero;
	uint8_t *start;
	size_t len;
	int rc;

	DBGC ( &context, "RSA %p decrypting:\n", &context );
	DBGC_HDA ( &context, 0, ciphertext->data, ciphertext->len );

	/* Initialise context */
	if ( ( rc = rsa_init ( &context, key ) ) != 0 )
		goto err_init;

	/* Sanity check */
	if ( ciphertext->len != context.max_len ) {
		DBGC ( &context, "RSA %p ciphertext incorrect length (%zd "
		       "bytes, should be %zd)\n",
		       &context, ciphertext->len, context.max_len );
		rc = -ERANGE;
		goto err_sanity;
	}

	/* Decipher the message (using the big integer input buffer as
	 * temporary storage)
	 */
	temp = context.input0;
	encoded = temp;
	rsa_cipher ( &context, ciphertext->data, encoded );

	/* Parse the message */
	end = ( encoded + context.max_len );
	if ( ( encoded[0] != 0x00 ) || ( encoded[1] != 0x02 ) ) {
		rc = -EINVAL;
		goto err_invalid;
	}
	zero = memchr ( &encoded[2], 0, ( end - &encoded[2] ) );
	if ( ! zero ) {
		DBGC ( &context, "RSA %p invalid decrypted message:\n",
		       &context );
		DBGC_HDA ( &context, 0, encoded, context.max_len );
		rc = -EINVAL;
		goto err_invalid;
	}
	start = ( zero + 1 );
	len = ( end - start );

	/* Create space for plaintext */
	if ( ( rc = asn1_grow ( plaintext, len ) ) != 0 )
		goto err_grow;

	/* Copy out message */
	memcpy ( plaintext->data, start, len );
	DBGC ( &context, "RSA %p decrypted:\n", &context );
	DBGC_HDA ( &context, 0, plaintext->data, len );

	/* Free context */
	rsa_free ( &context );

	return 0;

 err_grow:
 err_invalid:
 err_sanity:
	rsa_free ( &context );
 err_init:
	return rc;
}

/**
 * Encode RSA digest
 *
 * @v context		RSA context
 * @v digest		Digest algorithm
 * @v value		Digest value
 * @v encoded		Encoded digest
 * @ret rc		Return status code
 */
static int rsa_encode_digest ( struct rsa_context *context,
			       struct digest_algorithm *digest,
			       const void *value, void *encoded ) {
	struct rsa_digestinfo_prefix *prefix;
	size_t digest_len = digest->digestsize;
	uint8_t *temp = encoded;
	size_t digestinfo_len;
	size_t max_len;
	size_t pad_len;

	/* Identify prefix */
	prefix = rsa_find_prefix ( digest );
	if ( ! prefix ) {
		DBGC ( context, "RSA %p has no prefix for %s\n",
		       context, digest->name );
		return -ENOTSUP;
	}
	digestinfo_len = ( prefix->len + digest_len );

	/* Sanity check */
	max_len = ( context->max_len - 11 );
	if ( digestinfo_len > max_len ) {
		DBGC ( context, "RSA %p %s digestInfo too long (%zd bytes, "
		       "max %zd)\n", context, digest->name, digestinfo_len,
		       max_len );
		return -ERANGE;
	}
	DBGC ( context, "RSA %p encoding %s digest:\n",
	       context, digest->name );
	DBGC_HDA ( context, 0, value, digest_len );

	/* Construct encoded message */
	*(temp++) = 0x00;
	*(temp++) = 0x01;
	pad_len = ( max_len - digestinfo_len + 8 );
	memset ( temp, 0xff, pad_len );
	temp += pad_len;
	*(temp++) = 0x00;
	memcpy ( temp, prefix->data, prefix->len );
	temp += prefix->len;
	memcpy ( temp, value, digest_len );
	temp += digest_len;
	assert ( temp == ( encoded + context->max_len ) );
	DBGC ( context, "RSA %p encoded %s digest:\n", context, digest->name );
	DBGC_HDA ( context, 0, encoded, context->max_len );

	return 0;
}

/**
 * Sign digest value using RSA
 *
 * @v key		Key
 * @v digest		Digest algorithm
 * @v value		Digest value
 * @v signature		Signature
 * @ret rc		Return status code
 */
static int rsa_sign ( const struct asn1_cursor *key,
		      struct digest_algorithm *digest, const void *value,
		      struct asn1_builder *signature ) {
	struct rsa_context context;
	int rc;

	DBGC ( &context, "RSA %p signing %s digest:\n",
	       &context, digest->name );
	DBGC_HDA ( &context, 0, value, digest->digestsize );

	/* Initialise context */
	if ( ( rc = rsa_init ( &context, key ) ) != 0 )
		goto err_init;

	/* Create space for encoded digest and signature */
	if ( ( rc = asn1_grow ( signature, context.max_len ) ) != 0 )
		goto err_grow;

	/* Encode digest */
	if ( ( rc = rsa_encode_digest ( &context, digest, value,
					signature->data ) ) != 0 )
		goto err_encode;

	/* Encipher the encoded digest */
	rsa_cipher ( &context, signature->data, signature->data );
	DBGC ( &context, "RSA %p signed %s digest:\n", &context, digest->name );
	DBGC_HDA ( &context, 0, signature->data, signature->len );

	/* Free context */
	rsa_free ( &context );

	return 0;

 err_encode:
 err_grow:
	rsa_free ( &context );
 err_init:
	return rc;
}

/**
 * Verify signed digest value using RSA
 *
 * @v key		Key
 * @v digest		Digest algorithm
 * @v value		Digest value
 * @v signature		Signature
 * @ret rc		Return status code
 */
static int rsa_verify ( const struct asn1_cursor *key,
			struct digest_algorithm *digest, const void *value,
			const struct asn1_cursor *signature ) {
	struct rsa_context context;
	void *temp;
	void *expected;
	void *actual;
	int rc;

	DBGC ( &context, "RSA %p verifying %s digest:\n",
	       &context, digest->name );
	DBGC_HDA ( &context, 0, value, digest->digestsize );
	DBGC_HDA ( &context, 0, signature->data, signature->len );

	/* Initialise context */
	if ( ( rc = rsa_init ( &context, key ) ) != 0 )
		goto err_init;

	/* Sanity check */
	if ( signature->len != context.max_len ) {
		DBGC ( &context, "RSA %p signature incorrect length (%zd "
		       "bytes, should be %zd)\n",
		       &context, signature->len, context.max_len );
		rc = -ERANGE;
		goto err_sanity;
	}

	/* Decipher the signature (using the big integer input buffer
	 * as temporary storage)
	 */
	temp = context.input0;
	expected = temp;
	rsa_cipher ( &context, signature->data, expected );
	DBGC ( &context, "RSA %p deciphered signature:\n", &context );
	DBGC_HDA ( &context, 0, expected, context.max_len );

	/* Encode digest (using the big integer output buffer as
	 * temporary storage)
	 */
	temp = context.output0;
	actual = temp;
	if ( ( rc = rsa_encode_digest ( &context, digest, value,
					actual ) ) != 0 )
		goto err_encode;

	/* Verify the signature */
	if ( memcmp ( actual, expected, context.max_len ) != 0 ) {
		DBGC ( &context, "RSA %p signature verification failed\n",
		       &context );
		rc = -EACCES_VERIFY;
		goto err_verify;
	}

	/* Free context */
	rsa_free ( &context );

	DBGC ( &context, "RSA %p signature verified successfully\n", &context );
	return 0;

 err_verify:
 err_encode:
 err_sanity:
	rsa_free ( &context );
 err_init:
	return rc;
}

/**
 * Check for matching RSA public/private key pair
 *
 * @v private_key	Private key
 * @v public_key	Public key
 * @ret rc		Return status code
 */
static int rsa_match ( const struct asn1_cursor *private_key,
		       const struct asn1_cursor *public_key ) {
	struct asn1_cursor private_modulus;
	struct asn1_cursor private_exponent;
	struct asn1_cursor public_modulus;
	struct asn1_cursor public_exponent;
	int rc;

	/* Parse moduli and exponents */
	if ( ( rc = rsa_parse_mod_exp ( &private_modulus, &private_exponent,
					private_key ) ) != 0 )
		return rc;
	if ( ( rc = rsa_parse_mod_exp ( &public_modulus, &public_exponent,
					public_key ) ) != 0 )
		return rc;

	/* Compare moduli */
	if ( asn1_compare ( &private_modulus, &public_modulus ) != 0 )
		return -ENOTTY;

	return 0;
}

/** RSA public-key algorithm */
struct pubkey_algorithm rsa_algorithm = {
	.name		= "rsa",
	.encrypt	= rsa_encrypt,
	.decrypt	= rsa_decrypt,
	.sign		= rsa_sign,
	.verify		= rsa_verify,
	.match		= rsa_match,
};

/* Drag in objects via rsa_algorithm */
REQUIRING_SYMBOL ( rsa_algorithm );

/* Drag in crypto configuration */
REQUIRE_OBJECT ( config_crypto );
