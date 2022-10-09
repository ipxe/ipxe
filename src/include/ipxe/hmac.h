#ifndef _IPXE_HMAC_H
#define _IPXE_HMAC_H

/** @file
 *
 * Keyed-Hashing for Message Authentication
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/crypto.h>

/** HMAC context type */
#define hmac_context_t( digest ) struct {				\
		/** Digest context */					\
		uint8_t ctx[ digest->ctxsize ];				\
		/** HMAC input/output padding */			\
		uint8_t pad[ digest->blocksize ];			\
	} __attribute__ (( packed ))

/**
 * Calculate HMAC context size
 *
 * @v digest		Digest algorithm to use
 * @ret len		HMAC context size
 */
static inline __attribute__ (( always_inline )) size_t
hmac_ctxsize ( struct digest_algorithm *digest ) {
	hmac_context_t ( digest ) *hctx;

	return sizeof ( *hctx );
}

/**
 * Update HMAC
 *
 * @v digest		Digest algorithm to use
 * @v ctx		HMAC context
 * @v data		Data
 * @v len		Length of data
 */
static inline void hmac_update ( struct digest_algorithm *digest, void *ctx,
				 const void *data, size_t len ) {
	hmac_context_t ( digest ) *hctx = ctx;

	digest_update ( digest, hctx->ctx, data, len );
}

extern void hmac_init ( struct digest_algorithm *digest, void *ctx,
			const void *key, size_t key_len );
extern void hmac_final ( struct digest_algorithm *digest, void *ctx,
			 void *hmac );

#endif /* _IPXE_HMAC_H */
