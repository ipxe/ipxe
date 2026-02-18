#ifndef _IPXE_CRYPTO_H
#define _IPXE_CRYPTO_H

/** @file
 *
 * Cryptographic API
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <ipxe/asn1.h>

/** A message digest algorithm */
struct digest_algorithm {
	/** Algorithm name */
	const char *name;
	/** Context size */
	size_t ctxsize;
	/** Block size */
	size_t blocksize;
	/** Digest size */
	size_t digestsize;
	/** Initialise digest
	 *
	 * @v ctx		Context
	 */
	void ( * init ) ( void *ctx );
	/** Update digest with new data
	 *
	 * @v ctx		Context
	 * @v src		Data to digest
	 * @v len		Length of data
	 *
	 * @v len is not necessarily a multiple of @c blocksize.
	 */
	void ( * update ) ( void *ctx, const void *src, size_t len );
	/** Finalise digest
	 *
	 * @v ctx		Context
	 * @v out		Buffer for digest output
	 */
	void ( * final ) ( void *ctx, void *out );
};

/** A cipher algorithm */
struct cipher_algorithm {
	/** Algorithm name */
	const char *name;
	/** Context size */
	size_t ctxsize;
	/** Block size
	 *
	 * Every call to encrypt() or decrypt() must be for a multiple
	 * of this size.
	 */
	size_t blocksize;
	/** Alignment size
	 *
	 * Every call to encrypt() or decrypt() must begin at a
	 * multiple of this offset from the start of the stream.
	 * (Equivalently: all but the last call to encrypt() or
	 * decrypt() must be for a multiple of this size.)
	 *
	 * For ciphers supporting additional data, the main data
	 * stream and additional data stream are both considered to
	 * begin at offset zero.
	 */
	size_t alignsize;
	/** Authentication tag size */
	size_t authsize;
	/** Set key
	 *
	 * @v ctx	Context
	 * @v key	Key
	 * @v keylen	Key length
	 * @ret rc	Return status code
	 */
	int ( * setkey ) ( void *ctx, const void *key, size_t keylen );
	/** Set initialisation vector
	 *
	 * @v ctx	Context
	 * @v iv	Initialisation vector
	 * @v ivlen	Initialisation vector length
	 */
	void ( * setiv ) ( void *ctx, const void *iv, size_t ivlen );
	/** Encrypt data
	 *
	 * @v ctx	Context
	 * @v src	Data to encrypt
	 * @v dst	Buffer for encrypted data, or NULL for additional data
	 * @v len	Length of data
	 *
	 * @v len is guaranteed to be a multiple of @c blocksize.
	 */
	void ( * encrypt ) ( void *ctx, const void *src, void *dst,
			     size_t len );
	/** Decrypt data
	 *
	 * @v ctx	Context
	 * @v src	Data to decrypt
	 * @v dst	Buffer for decrypted data, or NULL for additional data
	 * @v len	Length of data
	 *
	 * @v len is guaranteed to be a multiple of @c blocksize.
	 */
	void ( * decrypt ) ( void *ctx, const void *src, void *dst,
			     size_t len );
	/** Generate authentication tag
	 *
	 * @v ctx	Context
	 * @v auth	Authentication tag
	 */
	void ( * auth ) ( void *ctx, void *auth );
};

/** A public key algorithm */
struct pubkey_algorithm {
	/** Algorithm name */
	const char *name;
	/** Encrypt
	 *
	 * @v key		Key
	 * @v plaintext		Plaintext
	 * @v ciphertext	Ciphertext
	 * @ret rc		Return status code
	 */
	int ( * encrypt ) ( const struct asn1_cursor *key,
			    const struct asn1_cursor *plaintext,
			    struct asn1_builder *ciphertext );
	/** Decrypt
	 *
	 * @v key		Key
	 * @v ciphertext	Ciphertext
	 * @v plaintext		Plaintext
	 * @ret rc		Return status code
	 */
	int ( * decrypt ) ( const struct asn1_cursor *key,
			    const struct asn1_cursor *ciphertext,
			    struct asn1_builder *plaintext );
	/** Sign digest value
	 *
	 * @v key		Key
	 * @v digest		Digest algorithm
	 * @v value		Digest value
	 * @v signature		Signature
	 * @ret rc		Return status code
	 */
	int ( * sign ) ( const struct asn1_cursor *key,
			 struct digest_algorithm *digest, const void *value,
			 struct asn1_builder *builder );
	/** Verify signed digest value
	 *
	 * @v key		Key
	 * @v digest		Digest algorithm
	 * @v value		Digest value
	 * @v signature		Signature
	 * @ret rc		Return status code
	 */
	int ( * verify ) ( const struct asn1_cursor *key,
			   struct digest_algorithm *digest, const void *value,
			   const struct asn1_cursor *signature );
	/** Check that public key matches private key
	 *
	 * @v private_key	Private key
	 * @v public_key	Public key
	 * @ret rc		Return status code
	 */
	int ( * match ) ( const struct asn1_cursor *private_key,
			  const struct asn1_cursor *public_key );
};

/** An elliptic curve */
struct elliptic_curve {
	/** Curve name */
	const char *name;
	/** Point (and public key) size */
	size_t pointsize;
	/** Scalar (and private key) size */
	size_t keysize;
	/** Generator base point */
	const void *base;
	/** Order of the generator (if prime) */
	const void *order;
	/** Check if this is the point at infinity
	 *
	 * @v point		Curve point
	 * @ret is_infinity	This is the point at infinity
	 *
	 * The point at infinity cannot be represented in affine
	 * coordinates.  Each curve must choose a representation of
	 * the point at infinity (e.g. all zeroes).
	 */
	int ( * is_infinity ) ( const void *point );
	/** Multiply scalar by curve point
	 *
	 * @v base		Base point
	 * @v scalar		Scalar multiple
	 * @v result		Result point to fill in
	 * @ret rc		Return status code
	 */
	int ( * multiply ) ( const void *base, const void *scalar,
			     void *result );
	/** Add curve points (as a one-off operation)
	 *
	 * @v addend		Curve point to add
	 * @v augend		Curve point to add
	 * @v result		Curve point to hold result
	 * @ret rc		Return status code
	 */
	int ( * add ) ( const void *addend, const void *augend, void *result );
};

static inline __attribute__ (( always_inline )) void
digest_init ( struct digest_algorithm *digest, void *ctx ) {
	digest->init ( ctx );
}

static inline __attribute__ (( always_inline )) void
digest_update ( struct digest_algorithm *digest, void *ctx,
		const void *data, size_t len ) {
	digest->update ( ctx, data, len );
}

static inline __attribute__ (( always_inline )) void
digest_final ( struct digest_algorithm *digest, void *ctx, void *out ) {
	digest->final ( ctx, out );
}

static inline __attribute__ (( always_inline )) int
cipher_setkey ( struct cipher_algorithm *cipher, void *ctx,
		const void *key, size_t keylen ) {
	return cipher->setkey ( ctx, key, keylen );
}

static inline __attribute__ (( always_inline )) void
cipher_setiv ( struct cipher_algorithm *cipher, void *ctx,
	       const void *iv, size_t ivlen ) {
	cipher->setiv ( ctx, iv, ivlen );
}

static inline __attribute__ (( always_inline )) void
cipher_encrypt ( struct cipher_algorithm *cipher, void *ctx,
		 const void *src, void *dst, size_t len ) {
	cipher->encrypt ( ctx, src, dst, len );
}
#define cipher_encrypt( cipher, ctx, src, dst, len ) do {		\
	assert ( ( (len) & ( (cipher)->blocksize - 1 ) ) == 0 );	\
	cipher_encrypt ( (cipher), (ctx), (src), (dst), (len) );	\
	} while ( 0 )

static inline __attribute__ (( always_inline )) void
cipher_decrypt ( struct cipher_algorithm *cipher, void *ctx,
		 const void *src, void *dst, size_t len ) {
	cipher->decrypt ( ctx, src, dst, len );
}
#define cipher_decrypt( cipher, ctx, src, dst, len ) do {		\
	assert ( ( (len) & ( (cipher)->blocksize - 1 ) ) == 0 );	\
	cipher_decrypt ( (cipher), (ctx), (src), (dst), (len) );	\
	} while ( 0 )

static inline __attribute__ (( always_inline )) void
cipher_auth ( struct cipher_algorithm *cipher, void *ctx, void *auth ) {
	cipher->auth ( ctx, auth );
}

static inline __attribute__ (( always_inline )) int
is_stream_cipher ( struct cipher_algorithm *cipher ) {
	return ( cipher->blocksize == 1 );
}

static inline __attribute__ (( always_inline )) int
is_block_cipher ( struct cipher_algorithm *cipher ) {
	return ( cipher->blocksize > 1 );
}

static inline __attribute__ (( always_inline )) int
is_auth_cipher ( struct cipher_algorithm *cipher ) {
	return cipher->authsize;
}

static inline __attribute__ (( always_inline )) int
pubkey_encrypt ( struct pubkey_algorithm *pubkey, const struct asn1_cursor *key,
		 const struct asn1_cursor *plaintext,
		 struct asn1_builder *ciphertext ) {
	return pubkey->encrypt ( key, plaintext, ciphertext );
}

static inline __attribute__ (( always_inline )) int
pubkey_decrypt ( struct pubkey_algorithm *pubkey, const struct asn1_cursor *key,
		 const struct asn1_cursor *ciphertext,
		 struct asn1_builder *plaintext ) {
	return pubkey->decrypt ( key, ciphertext, plaintext );
}

static inline __attribute__ (( always_inline )) int
pubkey_sign ( struct pubkey_algorithm *pubkey, const struct asn1_cursor *key,
	      struct digest_algorithm *digest, const void *value,
	      struct asn1_builder *signature ) {
	return pubkey->sign ( key, digest, value, signature );
}

static inline __attribute__ (( always_inline )) int
pubkey_verify ( struct pubkey_algorithm *pubkey, const struct asn1_cursor *key,
		struct digest_algorithm *digest, const void *value,
		const struct asn1_cursor *signature ) {
	return pubkey->verify ( key, digest, value, signature );
}

static inline __attribute__ (( always_inline )) int
pubkey_match ( struct pubkey_algorithm *pubkey,
	       const struct asn1_cursor *private_key,
	       const struct asn1_cursor *public_key ) {
	return pubkey->match ( private_key, public_key );
}

static inline __attribute__ (( always_inline )) int
elliptic_is_infinity ( struct elliptic_curve *curve, const void *point ) {
	return curve->is_infinity ( point );
}

static inline __attribute__ (( always_inline )) int
elliptic_multiply ( struct elliptic_curve *curve,
		    const void *base, const void *scalar, void *result ) {
	return curve->multiply ( base, scalar, result );
}

static inline __attribute__ (( always_inline )) int
elliptic_add ( struct elliptic_curve *curve, const void *addend,
	       const void *augend, void *result ) {
	return curve->add ( addend, augend, result );
}

extern void digest_null_init ( void *ctx );
extern void digest_null_update ( void *ctx, const void *src, size_t len );
extern void digest_null_final ( void *ctx, void *out );

extern int cipher_null_setkey ( void *ctx, const void *key, size_t keylen );
extern void cipher_null_setiv ( void *ctx, const void *iv, size_t ivlen );
extern void cipher_null_encrypt ( void *ctx, const void *src, void *dst,
				  size_t len );
extern void cipher_null_decrypt ( void *ctx, const void *src, void *dst,
				  size_t len );
extern void cipher_null_auth ( void *ctx, void *auth );

extern int pubkey_null_encrypt ( const struct asn1_cursor *key,
				 const struct asn1_cursor *plaintext,
				 struct asn1_builder *ciphertext );
extern int pubkey_null_decrypt ( const struct asn1_cursor *key,
				 const struct asn1_cursor *ciphertext,
				 struct asn1_builder *plaintext );
extern int pubkey_null_sign ( const struct asn1_cursor *key,
			      struct digest_algorithm *digest,
			      const void *value,
			      struct asn1_builder *signature );
extern int pubkey_null_verify ( const struct asn1_cursor *key,
				struct digest_algorithm *digest,
				const void *value,
				const struct asn1_cursor *signature );

extern struct digest_algorithm digest_null;
extern struct cipher_algorithm cipher_null;
extern struct pubkey_algorithm pubkey_null;

#endif /* _IPXE_CRYPTO_H */
