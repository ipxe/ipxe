#ifndef _GPXE_CBC_H
#define _GPXE_CBC_H

/** @file
 *
 * Cipher-block chaining
 *
 */

#include <gpxe/crypto.h>

/**
 * Set key
 *
 * @v ctx		Context
 * @v key		Key
 * @v keylen		Key length
 * @v cipher		Underlying cipher algorithm
 * @v cbc_ctx		CBC context
 * @ret rc		Return status code
 */
static inline int cbc_setkey ( void *ctx, const void *key, size_t keylen,
			       struct cipher_algorithm *cipher,
			       void *cbc_ctx __unused ) {

	return cipher_setkey ( cipher, ctx, key, keylen );
}

/**
 * Set initialisation vector
 *
 * @v ctx		Context
 * @v iv		Initialisation vector
 * @v cipher		Underlying cipher algorithm
 * @v cbc_ctx		CBC context
 */
static inline void cbc_setiv ( void *ctx __unused, const void *iv,
			       struct cipher_algorithm *cipher,
			       void *cbc_ctx ) {
	memcpy ( cbc_ctx, iv, cipher->blocksize );
}

extern void cbc_encrypt ( void *ctx, const void *src, void *dst,
			  size_t len, struct cipher_algorithm *cipher,
			  void *cbc_ctx );
extern void cbc_decrypt ( void *ctx, const void *src, void *dst,
			  size_t len, struct cipher_algorithm *cipher,
			  void *cbc_ctx );

#endif /* _GPXE_CBC_H */
