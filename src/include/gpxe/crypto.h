#ifndef _GPXE_CRYPTO_H
#define _GPXE_CRYPTO_H

/** @file
 *
 * Cryptographic API
 *
 */

#include <stdint.h>
#include <stddef.h>

/** A cryptographic algorithm */
struct crypto_algorithm {
	/** Algorithm name */
	const char *name;
	/** Context size */
	size_t ctxsize;
	/** Block size */
	size_t blocksize;
	/** Final output size */
	size_t digestsize;
	/** Initialise algorithm
	 *
	 * @v ctx		Context
	 */
	void ( * init ) ( void *ctx );
	/** Set key
	 *
	 * @v ctx		Context
	 * @v key		Key
	 * @v keylen		Key length
	 * @ret rc		Return status code
	 */
	int ( * setkey ) ( void *ctx, const void *key, size_t keylen );
	/** Set initialisation vector
	 *
	 * @v ctx		Context
	 * @v iv		Initialisation vector
	 */
	void ( *setiv ) ( void *ctx, const void *iv );
	/** Encode data
	 *
	 * @v ctx		Context
	 * @v src		Data to encode
	 * @v dst		Encoded data, or NULL
	 * @v len		Length of data
	 * @ret rc		Return status code
	 *
	 * For a cipher algorithm, the enciphered data should be
	 * placed in @c dst.  For a digest algorithm, only the digest
	 * state should be updated, and @c dst will be NULL.
	 *
	 * @v len is guaranteed to be a multiple of @c blocksize.
	 */
	void ( * encode ) ( void *ctx, const void *src, void *dst,
			    size_t len );
	/** Decode data
	 *
	 * @v ctx		Context
	 * @v src		Data to decode
	 * @v dst		Decoded data
	 * @v len		Length of data
	 * @ret rc		Return status code
	 *
	 * @v len is guaranteed to be a multiple of @c blocksize.
	 */
	void ( * decode ) ( void *ctx, const void *src, void *dst,
			    size_t len );
	/** Finalise algorithm
	 *
	 * @v ctx		Context
	 * @v out		Algorithm final output
	 */
	void ( * final ) ( void *ctx, void *out );
};

static inline void digest_init ( struct crypto_algorithm *crypto,
				 void *ctx ) {
	crypto->init ( ctx );
}

static inline void digest_update ( struct crypto_algorithm *crypto,
				   void *ctx, const void *data, size_t len ) {
	crypto->encode ( ctx, data, NULL, len );
}

static inline void digest_final ( struct crypto_algorithm *crypto,
				  void *ctx, void *out ) {
	crypto->final ( ctx, out );
}

static inline void cipher_setiv ( struct crypto_algorithm *crypto,
				  void *ctx, const void *iv ) {
	crypto->setiv ( ctx, iv );
}

static inline int cipher_setkey ( struct crypto_algorithm *crypto,
				  void *ctx, const void *key, size_t keylen ) {
	return crypto->setkey ( ctx, key, keylen );
}

static inline int is_stream_cipher ( struct crypto_algorithm *crypto ) {
	return ( crypto->blocksize == 1 );
}

extern struct crypto_algorithm crypto_null;

extern int cipher_encrypt ( struct crypto_algorithm *crypto,
			    void *ctx, const void *src, void *dst,
			    size_t len );
extern int cipher_decrypt ( struct crypto_algorithm *crypto,
			    void *ctx, const void *src, void *dst,
			    size_t len );

#endif /* _GPXE_CRYPTO_H */
