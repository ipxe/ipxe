/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
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

/** @file
 *
 * Elliptic curve digital signature algorithm (ECDSA)
 *
 * The elliptic curve public key format is documented in RFC 5480.
 * The original private key format is documented in RFC 5915, and the
 * generic container PKCS#8 format documented in RFC 5208.
 *
 */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ipxe/crypto.h>
#include <ipxe/bigint.h>
#include <ipxe/hmac_drbg.h>
#include <ipxe/ecdsa.h>

/* Disambiguate the various error causes */
#define EINVAL_POINTSIZE \
	__einfo_error ( EINFO_EINVAL_POINTSIZE )
#define EINFO_EINVAL_POINTSIZE \
	__einfo_uniqify ( EINFO_EINVAL, 0x01, "Invalid point size" )
#define EINVAL_KEYSIZE \
	__einfo_error ( EINFO_EINVAL_KEYSIZE )
#define EINFO_EINVAL_KEYSIZE \
	__einfo_uniqify ( EINFO_EINVAL, 0x02, "Invalid key size" )
#define EINVAL_COMPRESSION \
	__einfo_error ( EINFO_EINVAL_COMPRESSION )
#define EINFO_EINVAL_COMPRESSION \
	__einfo_uniqify ( EINFO_EINVAL, 0x03, "Invalid compression")
#define EINVAL_INFINITY \
	__einfo_error ( EINFO_EINVAL_INFINITY )
#define EINFO_EINVAL_INFINITY \
	__einfo_uniqify ( EINFO_EINVAL, 0x04, "Point is infinity" )
#define EINVAL_SIGNATURE \
	__einfo_error ( EINFO_EINVAL_SIGNATURE )
#define EINFO_EINVAL_SIGNATURE \
	__einfo_uniqify ( EINFO_EINVAL, 0x05, "Invalid signature" )

/** "ecPublicKey" object identifier */
static uint8_t oid_ecpublickey[] = { ASN1_OID_ECPUBLICKEY };

/** Generic elliptic curve container algorithm
 *
 * The actual curve to be used is identified via the algorithm
 * parameters, rather than the top-level OID.
 */
struct asn1_algorithm ecpubkey_algorithm __asn1_algorithm = {
	.name = "ecPublicKey",
	.oid = ASN1_CURSOR ( oid_ecpublickey ),
	.pubkey = &ecdsa_algorithm,
};

/** An ECDSA key */
struct ecdsa_key {
	/** Elliptic curve */
	struct elliptic_curve *curve;
	/** Public curve point */
	const void *public;
	/** Private multiple of base curve point (if applicable) */
	const void *private;
};

/** ECDSA context */
struct ecdsa_context {
	/** Key */
	struct ecdsa_key key;
	/** Big integer size */
	unsigned int size;
	/** Digest algorithm */
	struct digest_algorithm *digest;
	/** Digest length */
	size_t zlen;

	/** Dynamically allocated storage */
	void *dynamic;
	/** Element 0 of modulus N (i.e. curve group order */
	bigint_element_t *modulus0;
	/** Element 0 of constant N-2 (for Fermat's little theorem) */
	bigint_element_t *fermat0;
	/** Element 0 of Montgomery constant R^2 mod N */
	bigint_element_t *square0;
	/** Element 0 of constant 1 (in Montgomery form) */
	bigint_element_t *one0;
	/** Element 0 of digest value "z" */
	bigint_element_t *z0;
	/** Element 0 of random key "k" */
	bigint_element_t *k0;
	/** Element 0 of signature value "r" */
	bigint_element_t *r0;
	/** Element 0 of signature value "s" */
	bigint_element_t *s0;
	/** Element 0 of temporary value */
	bigint_element_t *temp0;
	/** Element 0 of product buffer */
	bigint_element_t *product0;
	/** Curve point 1 */
	void *point1;
	/** Curve point 2 */
	void *point2;
	/** Scalar value */
	void *scalar;
	/** HMAC_DRBG state for random value generation */
	struct hmac_drbg_state *drbg;
};

/**
 * Parse ECDSA key
 *
 * @v key		ECDSA key
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int ecdsa_parse_key ( struct ecdsa_key *key,
			     const struct asn1_cursor *raw ) {
	struct asn1_algorithm *algorithm;
	struct asn1_cursor cursor;
	struct asn1_cursor curve;
	struct asn1_cursor private;
	const uint8_t *compression;
	int is_private;
	int rc;

	/* Enter subjectPublicKeyInfo/ECPrivateKey */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_SEQUENCE );
	asn1_invalidate_cursor ( &curve );
	asn1_invalidate_cursor ( &private );

	/* Determine key format */
	if ( asn1_type ( &cursor ) == ASN1_INTEGER ) {

		/* Private key */
		is_private = 1;

		/* Skip version */
		asn1_skip_any ( &cursor );

		/* Parse privateKeyAlgorithm, if present */
		if ( asn1_type ( &cursor ) == ASN1_SEQUENCE ) {

			/* PKCS#8 format */
			DBGC ( key, "ECDSA %p is in PKCS#8 format\n", key );

			/* Parse privateKeyAlgorithm */
			memcpy ( &curve, &cursor, sizeof ( curve ) );
			asn1_skip_any ( &cursor );

			/* Enter privateKey */
			asn1_enter ( &cursor, ASN1_OCTET_STRING );

			/* Enter ECPrivateKey */
			asn1_enter ( &cursor, ASN1_SEQUENCE );

			/* Skip version */
			asn1_skip ( &cursor, ASN1_INTEGER );
		}

		/* Parse privateKey */
		memcpy ( &private, &cursor, sizeof ( private ) );
		asn1_enter ( &private, ASN1_OCTET_STRING );
		asn1_skip_any ( &cursor );

		/* Parse parameters, if present */
		if ( asn1_type ( &cursor ) == ASN1_EXPLICIT_TAG ( 0 ) ) {
			memcpy ( &curve, &cursor, sizeof ( curve ) );
			asn1_enter_any ( &curve );
			asn1_skip_any ( &cursor );
		}

		/* Enter publicKey */
		asn1_enter ( &cursor, ASN1_EXPLICIT_TAG ( 1 ) );

	} else {

		/* Public key */
		is_private = 0;

		/* Parse algorithm */
		memcpy ( &curve, &cursor, sizeof ( curve ) );
		asn1_skip_any ( &cursor );
	}

	/* Enter publicKey */
	asn1_enter_bits ( &cursor, NULL );

	/* Identify curve */
	if ( ( rc = asn1_curve_algorithm ( &curve, &ecpubkey_algorithm,
					   &algorithm ) ) != 0 ) {
		DBGC ( key, "ECDSA %p unknown curve: %s\n",
		       key, strerror ( rc ) );
		DBGC_HDA ( key, 0, raw->data, raw->len );
		return rc;
	}
	key->curve = algorithm->curve;
	DBGC ( key, "ECDSA %p is a %s (%s) %s key\n", key, algorithm->name,
	       key->curve->name, ( is_private ? "private" : "public" ) );

	/* Check public key length */
	if ( cursor.len != ( sizeof ( *compression ) +
			     key->curve->pointsize ) ) {
		DBGC ( key, "ECDSA %p invalid public key length %zd\n",
		       key, cursor.len );
		DBGC_HDA ( key, 0, raw->data, raw->len );
		return -EINVAL_POINTSIZE;
	}

	/* Check that key is uncompressed */
	compression = cursor.data;
	if ( *compression != ECDSA_UNCOMPRESSED ) {
		DBGC ( key, "ECDSA %p invalid compression %#02x\n",
		       key, *compression );
		DBGC_HDA ( key, 0, raw->data, raw->len );
		return -EINVAL_COMPRESSION;
	}

	/* Extract public curve point */
	key->public = ( cursor.data + sizeof ( *compression ) );
	DBGC ( key, "ECDSA %p public curve point:\n", key );
	DBGC_HDA ( key, 0, key->public, key->curve->pointsize );

	/* Check that public key is not the point at infinity */
	if ( elliptic_is_infinity ( key->curve, key->public ) ) {
		DBGC ( key, "ECDSA %p public curve point is infinity\n", key );
		return -EINVAL_INFINITY;
	}

	/* Extract private key, if applicable */
	if ( is_private ) {

		/* Check private key length */
		if ( private.len != key->curve->keysize ) {
			DBGC ( key, "ECDSA %p invalid private key length "
			       "%zd\n", key, private.len );
			DBGC_HDA ( key, 0, raw->data, raw->len );
			return -EINVAL_KEYSIZE;
		}

		/* Extract private key */
		key->private = private.data;
		DBGC ( key, "ECDSA %p private multiplier:\n", key );
		DBGC_HDA ( key, 0, key->private, key->curve->keysize );

	} else {

		/* No private key */
		key->private = NULL;
	}

	return 0;
}

/**
 * Parse ECDSA signature value
 *
 * @v ctx		ECDSA context
 * @v rs0		Element 0 of signature "r" or "s" value
 * @v raw		ASN.1 cursor
 * @ret rc		Return status code
 */
static int ecdsa_parse_signature ( struct ecdsa_context *ctx,
				   bigint_element_t *rs0,
				   const struct asn1_cursor *raw ) {
	size_t keysize = ctx->key.curve->keysize;
	unsigned int size = ctx->size;
	bigint_t ( size ) __attribute__ (( may_alias )) *modulus =
		( ( void * ) ctx->modulus0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *rs =
		( ( void * ) rs0 );
	struct asn1_cursor cursor;
	int rc;

	/* Enter integer */
	memcpy ( &cursor, raw, sizeof ( cursor ) );
	if ( ( rc = asn1_enter_unsigned ( &cursor ) ) != 0 ) {
		DBGC ( ctx, "ECDSA %p invalid integer:\n", ctx );
		DBGC_HDA ( ctx, 0, raw->data, raw->len );
		return rc;
	}

	/* Extract value */
	if ( cursor.len > keysize ) {
		DBGC ( ctx, "ECDSA %p invalid signature value:\n", ctx );
		DBGC_HDA ( ctx, 0, raw->data, raw->len );
		return -EINVAL_KEYSIZE;
	}
	bigint_init ( rs, cursor.data, cursor.len );

	/* Check that value is within the required range */
	if ( bigint_is_zero ( rs ) || bigint_is_geq ( rs, modulus ) ) {
		DBGC ( ctx, "ECDSA %p out-of-range signature value:\n", ctx );
		DBGC_HDA ( ctx, 0, raw->data, raw->len );
		return -ERANGE;
	}

	return 0;
}

/**
 * Prepend ECDSA signature value
 *
 * @v ctx		ECDSA context
 * @v rs0		Element 0 of signature "r" or "s" value
 * @v builder		ASN.1 builder
 * @ret rc		Return status code
 */
static int ecdsa_prepend_signature ( struct ecdsa_context *ctx,
				     bigint_element_t *rs0,
				     struct asn1_builder *builder ) {
	size_t keysize = ctx->key.curve->keysize;
	unsigned int size = ctx->size;
	bigint_t ( size ) __attribute__ (( may_alias )) *rs =
		( ( void * ) rs0 );
	uint8_t buf[ 1 /* potential sign byte */ + keysize ];
	uint8_t *data;
	size_t len;
	int rc;

	/* Construct value */
	buf[0] = 0;
	bigint_done ( rs, &buf[1], keysize );

	/* Strip leading zeros */
	data = buf;
	len = sizeof ( buf );
	while ( ( len > 1 ) && ( data[0] == 0 ) && ( data[1] < 0x80 ) ) {
		data++;
		len--;
	}

	/* Prepend integer */
	if ( ( rc = asn1_prepend ( builder, ASN1_INTEGER, data, len ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Allocate ECDSA context dynamic storage
 *
 * @v ctx		ECDSA context
 * @ret rc		Return status code
 */
static int ecdsa_alloc ( struct ecdsa_context *ctx ) {
	struct elliptic_curve *curve = ctx->key.curve;
	size_t pointsize = curve->pointsize;
	size_t keysize = curve->keysize;
	unsigned int size =
		bigint_required_size ( keysize + 1 /* for addition */ );
	struct {
		bigint_t ( size ) modulus;
		bigint_t ( size ) fermat;
		bigint_t ( size ) square;
		bigint_t ( size ) one;
		bigint_t ( size ) z;
		bigint_t ( size ) k;
		bigint_t ( size ) r;
		bigint_t ( size ) s;
		bigint_t ( size ) temp;
		bigint_t ( size * 2 ) product;
		uint8_t point1[pointsize];
		uint8_t point2[pointsize];
		uint8_t scalar[keysize];
		struct hmac_drbg_state drbg;
	} *dynamic;

	/* Allocate dynamic storage */
	dynamic = malloc ( sizeof ( *dynamic ) );
	if ( ! dynamic )
		return -ENOMEM;

	/* Populate context */
	ctx->size = size;
	ctx->dynamic = dynamic;
	ctx->modulus0 = dynamic->modulus.element;
	ctx->fermat0 = dynamic->fermat.element;
	ctx->square0 = dynamic->square.element;
	ctx->one0 = dynamic->one.element;
	ctx->z0 = dynamic->z.element;
	ctx->k0 = dynamic->k.element;
	ctx->r0 = dynamic->r.element;
	ctx->s0 = dynamic->s.element;
	ctx->temp0 = dynamic->temp.element;
	ctx->product0 = dynamic->product.element;
	ctx->point1 = dynamic->point1;
	ctx->point2 = dynamic->point2;
	ctx->scalar = dynamic->scalar;
	ctx->drbg = &dynamic->drbg;

	return 0;
}

/**
 * Free ECDSA context dynamic storage
 *
 * @v ctx		ECDSA context
 */
static void ecdsa_free ( struct ecdsa_context *ctx ) {

	/* Free dynamic storage */
	free ( ctx->dynamic );
}

/**
 * Initialise ECDSA values
 *
 * @v ctx		ECDSA context
 * @v digest		Digest algorithm
 * @v value		Digest value
 */
static void ecdsa_init_values ( struct ecdsa_context *ctx,
				struct digest_algorithm *digest,
				const void *value ) {
	struct elliptic_curve *curve = ctx->key.curve;
	unsigned int size = ctx->size;
	bigint_t ( size ) __attribute__ (( may_alias )) *modulus =
		( ( void * ) ctx->modulus0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *fermat =
		( ( void * ) ctx->fermat0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *square =
		( ( void * ) ctx->square0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *one =
		( ( void * ) ctx->one0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *z =
		( ( void * ) ctx->z0 );
	bigint_t ( size * 2 ) __attribute__ (( may_alias )) *product =
		( ( void * ) ctx->product0 );
	static const uint8_t two_raw[] = { 2 };
	size_t zlen;

	/* Initialise modulus N */
	bigint_init ( modulus, curve->order, curve->keysize );
	DBGC2 ( ctx, "ECDSA %p       N = %s\n", ctx, bigint_ntoa ( modulus ) );

	/* Calculate N-2 (using Montgomery constant as temporary buffer) */
	bigint_copy ( modulus, fermat );
	bigint_init ( square, two_raw, sizeof ( two_raw ) );
	bigint_subtract ( square, fermat );

	/* Calculate Montgomery constant */
	bigint_reduce ( modulus, square );
	DBGC2 ( ctx, "ECDSA %p     R^2 = %s mod N\n",
		ctx, bigint_ntoa ( square ) );

	/* Construct one in Montgomery form */
	bigint_grow ( square, product );
	bigint_montgomery ( modulus, product, one );
	DBGC2 ( ctx, "ECDSA %p       R = %s mod N\n",
		ctx, bigint_ntoa ( one ) );

	/* Initialise digest */
	ctx->digest = digest;
	zlen = ctx->key.curve->keysize;
	if ( zlen > digest->digestsize )
		zlen = digest->digestsize;
	ctx->zlen = zlen;
	bigint_init ( z, value, zlen );
	DBGC2 ( ctx, "ECDSA %p       z = %s (%s)\n",
		ctx, bigint_ntoa ( z ), digest->name );
}

/**
 * Initialise ECDSA context
 *
 * @v ctx		ECDSA context
 * @v key		Key
 * @v digest		Digest algorithm
 * @v value		Digest value
 * @ret rc		Return status code
 */
static int ecdsa_init ( struct ecdsa_context *ctx,
			const struct asn1_cursor *key,
			struct digest_algorithm *digest,
			const void *value ) {
	int rc;

	/* Parse key */
	if ( ( rc = ecdsa_parse_key ( &ctx->key, key ) ) != 0 )
		goto err_parse;

	/* Allocate dynamic storage */
	if ( ( rc = ecdsa_alloc ( ctx ) ) != 0 )
		goto err_alloc;

	/* Initialise values */
	ecdsa_init_values ( ctx, digest, value );

	return 0;

	ecdsa_free ( ctx );
 err_alloc:
 err_parse:
	return rc;
}

/**
 * Invert ECDSA value
 *
 * @v ctx		ECDSA context
 * @v val0		Element 0 of value to invert
 */
static void ecdsa_invert ( struct ecdsa_context *ctx,
			   bigint_element_t *val0 ) {
	unsigned int size = ctx->size;
	bigint_t ( size ) __attribute__ (( may_alias )) *modulus =
		( ( void * ) ctx->modulus0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *fermat =
		( ( void * ) ctx->fermat0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *square =
		( ( void * ) ctx->square0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *one =
		( ( void * ) ctx->one0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *temp =
		( ( void * ) ctx->temp0 );
	bigint_t ( size * 2 ) __attribute__ (( may_alias )) *product =
		( ( void * ) ctx->product0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *val =
		( ( void * ) val0 );

	/* Convert value to Montgomery form */
	bigint_multiply ( val, square, product );
	bigint_montgomery ( modulus, product, temp );

	/* Invert value via Fermat's little theorem */
	bigint_copy ( one, val );
	bigint_ladder ( val, temp, fermat, bigint_mod_exp_ladder, modulus,
			product );
}

/**
 * Generate ECDSA "r" and "s" values
 *
 * @v ctx		ECDSA context
 * @v sig		Signature
 * @ret rc		Return status code
 */
static int ecdsa_sign_rs ( struct ecdsa_context *ctx ) {
	struct digest_algorithm *digest = ctx->digest;
	struct elliptic_curve *curve = ctx->key.curve;
	size_t pointsize = curve->pointsize;
	size_t keysize = curve->keysize;
	unsigned int size = ctx->size;
	bigint_t ( size ) __attribute__ (( may_alias )) *modulus =
		( ( void * ) ctx->modulus0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *square =
		( ( void * ) ctx->square0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *one =
		( ( void * ) ctx->one0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *z =
		( ( void * ) ctx->z0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *k =
		( ( void * ) ctx->k0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *r =
		( ( void * ) ctx->r0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *s =
		( ( void * ) ctx->s0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *temp =
		( ( void * ) ctx->temp0 );
	bigint_t ( size * 2 ) __attribute__ (( may_alias )) *product =
		( ( void * ) ctx->product0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *x1 =
		( ( void * ) temp );
	void *point1 = ctx->point1;
	void *scalar = ctx->scalar;
	int rc;

	/* Loop until a suitable signature is generated */
	while ( 1 ) {

		/* Generate pseudo-random data */
		if ( ( rc = hmac_drbg_generate ( digest, ctx->drbg, NULL, 0,
						 scalar, keysize ) ) != 0 ) {
			DBGC ( ctx, "ECDSA %p could not generate: %s\n",
			       ctx, strerror ( rc ) );
			return rc;
		}

		/* Check suitability of pseudo-random data */
		bigint_init ( k, scalar, keysize );
		DBGC2 ( ctx, "ECDSA %p       k = %s\n",
			ctx, bigint_ntoa ( k ) );
		if ( bigint_is_zero ( k ) )
			continue;
		if ( bigint_is_geq ( k, modulus ) )
			continue;

		/* Calculate (x1,y1) = k*G */
		elliptic_multiply ( curve, curve->base, scalar, point1 );
		bigint_init ( x1, point1, ( pointsize / 2 ) );
		DBGC2 ( ctx, "ECDSA %p      x1 = %s mod N\n",
			ctx, bigint_ntoa ( x1 ) );

		/* Calculate r = x1 mod N */
		bigint_multiply ( x1, one, product );
		bigint_montgomery ( modulus, product, r );
		DBGC2 ( ctx, "ECDSA %p       r = %s\n",
			ctx, bigint_ntoa ( r ) );

		/* Check suitability of r */
		if ( bigint_is_zero ( r ) )
			continue;

		/* Calculate k^-1 mod N (in Montgomery form) */
		ecdsa_invert ( ctx, k->element );
		DBGC2 ( ctx, "ECDSA %p (k^-1)R = %s mod N\n",
			ctx, bigint_ntoa ( k ) );

		/* Calculate r * dA */
		bigint_init ( temp, ctx->key.private, keysize );
		DBGC2 ( ctx, "ECDSA %p      dA = %s\n",
			ctx, bigint_ntoa ( temp ) );
		bigint_multiply ( r, temp, product );
		bigint_montgomery ( modulus, product, temp );
		bigint_multiply ( temp, square, product );
		bigint_montgomery ( modulus, product, temp );
		DBGC2 ( ctx, "ECDSA %p    r*dA = %s mod N\n",
			ctx, bigint_ntoa ( temp ) );

		/* Calculate k^-1 * (z + r*dA) */
		bigint_add ( z, temp );
		DBGC2 ( ctx, "ECDSA %p  z+r*dA = %s mod N\n",
			ctx, bigint_ntoa ( temp ) );
		bigint_multiply ( k, temp, product );
		bigint_montgomery ( modulus, product, s );
		DBGC2 ( ctx, "ECDSA %p       s = %s\n",
			ctx, bigint_ntoa ( s ) );

		/* Check suitability of s */
		if ( bigint_is_zero ( s ) )
			continue;

		return 0;
	}
}

/**
 * Verify ECDSA "r" and "s" values
 *
 * @v ctx		ECDSA context
 * @v sig		Signature
 * @ret rc		Return status code
 */
static int ecdsa_verify_rs ( struct ecdsa_context *ctx ) {
	struct elliptic_curve *curve = ctx->key.curve;
	size_t pointsize = curve->pointsize;
	size_t keysize = curve->keysize;
	const void *public = ctx->key.public;
	unsigned int size = ctx->size;
	bigint_t ( size ) __attribute__ (( may_alias )) *modulus =
		( ( void * ) ctx->modulus0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *one =
		( ( void * ) ctx->one0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *z =
		( ( void * ) ctx->z0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *r =
		( ( void * ) ctx->r0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *s =
		( ( void * ) ctx->s0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *temp =
		( ( void * ) ctx->temp0 );
	bigint_t ( size * 2 ) __attribute__ (( may_alias )) *product =
		( ( void * ) ctx->product0 );
	bigint_t ( size ) __attribute__ (( may_alias )) *u1 =
		( ( void * ) temp );
	bigint_t ( size ) __attribute__ (( may_alias )) *u2 =
		( ( void * ) temp );
	bigint_t ( size ) __attribute__ (( may_alias )) *x1 =
		( ( void * ) temp );
	void *point1 = ctx->point1;
	void *point2 = ctx->point2;
	void *scalar = ctx->scalar;
	int valid;
	int rc;

	DBGC2 ( ctx, "ECDSA %p       r = %s\n", ctx, bigint_ntoa ( r ) );
	DBGC2 ( ctx, "ECDSA %p       s = %s\n", ctx, bigint_ntoa ( s ) );

	/* Calculate s^-1 mod N (in Montgomery form) */
	ecdsa_invert ( ctx, s->element );
	DBGC2 ( ctx, "ECDSA %p (s^-1)R = %s mod N\n", ctx, bigint_ntoa ( s ) );

	/* Calculate u1 = (z * s^-1) mod N */
	bigint_multiply ( z, s, product );
	bigint_montgomery ( modulus, product, u1 );
	DBGC2 ( ctx, "ECDSA %p      u1 = %s mod N\n",
		ctx, bigint_ntoa ( u1 ) );
	bigint_done ( u1, scalar, keysize );

	/* Calculate u1 * G */
	if ( ( rc = elliptic_multiply ( curve, curve->base, scalar,
					point1 ) ) != 0 ) {
		DBGC ( ctx, "ECDSA %p could not calculate u1*G: %s\n",
		       ctx, strerror ( rc ) );
		return rc;
	}

	/* Calculate u2 = (r * s^-1) mod N */
	bigint_multiply ( r, s, product );
	bigint_montgomery ( modulus, product, u2 );
	bigint_done ( u2, scalar, keysize );
	DBGC2 ( ctx, "ECDSA %p      u2 = %s mod N\n",
		ctx, bigint_ntoa ( u2 ) );

	/* Calculate u2 * Qa */
	if ( ( rc = elliptic_multiply ( curve, public, scalar,
					point2 ) ) != 0 ) {
		DBGC ( ctx, "ECDSA %p could not calculate u2*Qa: %s\n",
		       ctx, strerror ( rc ) );
		return rc;
	}

	/* Calculate u1 * G + u2 * Qa */
	if ( ( rc = elliptic_add ( curve, point1, point2, point1 ) ) != 0 ) {
		DBGC ( ctx, "ECDSA %p could not calculate u1*G+u2*Qa: %s\n",
		       ctx, strerror ( rc ) );
		return rc;
	}

	/* Check that result is not the point at infinity */
	if ( elliptic_is_infinity ( curve, point1 ) ) {
		DBGC ( ctx, "ECDSA %p result is point at infinity\n", ctx );
		return -EINVAL;
	}

	/* Calculate x1 mod N */
	bigint_init ( x1, point1, ( pointsize / 2 ) );
	DBGC2 ( ctx, "ECDSA %p      x1 = %s mod N\n", ctx, bigint_ntoa ( x1 ) );
	bigint_multiply ( x1, one, product );
	bigint_montgomery ( modulus, product, x1 );
	DBGC2 ( ctx, "ECDSA %p      x1 = %s\n", ctx, bigint_ntoa ( x1 ) );

	/* Check signature */
	bigint_subtract ( x1, r );
	valid = bigint_is_zero ( r );
	DBGC2 ( ctx, "ECDSA %p signature is%s valid\n",
		ctx, ( valid ? "" : " not" ) );

	return ( valid ? 0 : -EINVAL_SIGNATURE );
}

/**
 * Encrypt using ECDSA
 *
 * @v key		Key
 * @v plaintext		Plaintext
 * @v ciphertext	Ciphertext
 * @ret rc		Return status code
 */
static int ecdsa_encrypt ( const struct asn1_cursor *key __unused,
			   const struct asn1_cursor *plaintext __unused,
			   struct asn1_builder *ciphertext __unused ) {

	/* Not a defined operation for ECDSA */
	return -ENOTTY;
}

/**
 * Decrypt using ECDSA
 *
 * @v key		Key
 * @v ciphertext	Ciphertext
 * @v plaintext		Plaintext
 * @ret rc		Return status code
 */
static int ecdsa_decrypt ( const struct asn1_cursor *key __unused,
			   const struct asn1_cursor *ciphertext __unused,
			   struct asn1_builder *plaintext __unused ) {

	/* Not a defined operation for ECDSA */
	return -ENOTTY;
}

/**
 * Sign digest value using ECDSA
 *
 * @v key		Key
 * @v digest		Digest algorithm
 * @v value		Digest value
 * @v signature		Signature
 * @ret rc		Return status code
 */
static int ecdsa_sign ( const struct asn1_cursor *key,
			struct digest_algorithm *digest, const void *value,
			struct asn1_builder *signature ) {
	struct ecdsa_context ctx;
	int rc;

	/* Initialise context */
	if ( ( rc = ecdsa_init ( &ctx, key, digest, value ) ) != 0 )
		goto err_init;

	/* Fail unless we have a private key */
	if ( ! ctx.key.private ) {
		rc = -ENOTTY;
		goto err_no_key;
	}

	/* Instantiate DRBG */
	hmac_drbg_instantiate ( digest, ctx.drbg, ctx.key.private,
				ctx.key.curve->keysize, value, ctx.zlen );

	/* Create signature */
	if ( ( rc = ecdsa_sign_rs ( &ctx ) ) != 0 )
		goto err_signature;

	/* Construct "r" and "s" values */
	if ( ( rc = ecdsa_prepend_signature ( &ctx, ctx.s0, signature ) ) != 0)
		goto err_s;
	if ( ( rc = ecdsa_prepend_signature ( &ctx, ctx.r0, signature ) ) != 0)
		goto err_r;
	if ( ( rc = asn1_wrap ( signature, ASN1_SEQUENCE ) ) != 0 )
		goto err_wrap;

	/* Free context */
	ecdsa_free ( &ctx );

	return 0;

 err_wrap:
 err_r:
 err_s:
 err_signature:
 err_no_key:
	ecdsa_free ( &ctx );
 err_init:
	return rc;
}

/**
 * Verify signed digest using ECDSA
 *
 * @v key		Key
 * @v digest		Digest algorithm
 * @v value		Digest value
 * @v signature		Signature
 * @ret rc		Return status code
 */
static int ecdsa_verify ( const struct asn1_cursor *key,
			  struct digest_algorithm *digest, const void *value,
			  const struct asn1_cursor *signature ) {
	struct ecdsa_context ctx;
	struct asn1_cursor cursor;
	int rc;

	/* Initialise context */
	if ( ( rc = ecdsa_init ( &ctx, key, digest, value ) ) != 0 )
		goto err_init;

	/* Enter sequence */
	memcpy ( &cursor, signature, sizeof ( cursor ) );
	asn1_enter ( &cursor, ASN1_SEQUENCE );

	/* Extract "r" and "s" values */
	if ( ( rc = ecdsa_parse_signature ( &ctx, ctx.r0, &cursor ) ) != 0 )
		goto err_r;
	asn1_skip_any ( &cursor );
	if ( ( rc = ecdsa_parse_signature ( &ctx, ctx.s0, &cursor ) ) != 0 )
		goto err_s;

	/* Verify signature */
	if ( ( rc = ecdsa_verify_rs ( &ctx ) ) != 0 )
		goto err_verify;

	/* Free context */
	ecdsa_free ( &ctx );

	return 0;

 err_verify:
 err_s:
 err_r:
	ecdsa_free ( &ctx );
 err_init:
	return rc;
}

/**
 * Check for matching ECDSA public/private key pair
 *
 * @v private_key	Private key
 * @v public_key	Public key
 * @ret rc		Return status code
 */
static int ecdsa_match ( const struct asn1_cursor *private_key,
			 const struct asn1_cursor *public_key ) {
	struct elliptic_curve *curve;
	struct ecdsa_key privkey;
	struct ecdsa_key pubkey;
	int rc;

	/* Parse keys */
	if ( ( rc = ecdsa_parse_key ( &privkey, private_key ) ) != 0 )
		return rc;
	if ( ( rc = ecdsa_parse_key ( &pubkey, public_key ) ) != 0 )
		return rc;

	/* Compare curves */
	if ( privkey.curve != pubkey.curve )
		return -ENOTTY;
	curve = privkey.curve;

	/* Compare public curve points */
	if ( memcmp ( privkey.public, pubkey.public, curve->pointsize ) != 0 )
		return -ENOTTY;

	return 0;
}

/** ECDSA public-key algorithm */
struct pubkey_algorithm ecdsa_algorithm = {
	.name		= "ecdsa",
	.encrypt	= ecdsa_encrypt,
	.decrypt	= ecdsa_decrypt,
	.sign		= ecdsa_sign,
	.verify		= ecdsa_verify,
	.match		= ecdsa_match,
};
