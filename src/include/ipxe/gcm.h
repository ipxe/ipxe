#ifndef _IPXE_GCM_H
#define _IPXE_GCM_H

/** @file
 *
 * Galois/Counter Mode (GCM)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>
#include <ipxe/crypto.h>

/** A GCM counter */
struct gcm_counter {
	/** Initialisation vector */
	uint8_t iv[12];
	/** Counter value */
	uint32_t value;
} __attribute__ (( packed ));

/** A GCM length pair */
struct gcm_lengths {
	/** Additional data length */
	uint64_t add;
	/** Data length */
	uint64_t data;
} __attribute__ (( packed ));

/** A GCM block */
union gcm_block {
	/** Raw bytes */
	uint8_t byte[16];
	/** Raw words */
	uint16_t word[8];
	/** Raw dwords */
	uint32_t dword[4];
	/** Counter */
	struct gcm_counter ctr;
	/** Lengths */
	struct gcm_lengths len;
} __attribute__ (( packed ));

/** GCM context */
struct gcm_context {
	/** Accumulated hash (X) */
	union gcm_block hash;
	/** Accumulated lengths */
	union gcm_block len;
	/** Counter (Y) */
	union gcm_block ctr;
	/** Hash key (H) */
	union gcm_block key;
	/** Processing flags */
	unsigned int flags;
};

/** A GCM mode context */
#define gcm_context_t( ctxsize )					\
	struct {							\
		struct gcm_context gcm;					\
		uint8_t raw[ (ctxsize) -				\
			     sizeof ( struct gcm_context ) ];		\
	}

extern int gcm_setkey ( struct cipher_algorithm *cipher, void *ctx,
			const void *key, size_t keylen );
extern int gcm_setiv ( struct cipher_algorithm *cipher, void *ctx,
		       const void *iv, size_t ivlen );
extern void gcm_encrypt ( struct cipher_algorithm *cipher, void *ctx,
			  const void *src, void *dst, size_t len );
extern void gcm_decrypt ( struct cipher_algorithm *cipher, void *ctx,
			  const void *src, void *dst, size_t len );
extern void gcm_auth ( struct cipher_algorithm *cipher, void *ctx,
		       void *auth );

/**
 * Create a GCM mode of behaviour of an existing cipher
 *
 * @v _cbc_name		Name for the new CBC cipher
 * @v _cbc_cipher	New cipher algorithm
 * @v _raw_cipher	Underlying cipher algorithm
 * @v _raw_context	Context structure for the underlying cipher
 * @v _blocksize	Cipher block size
 */
#define GCM_CIPHER( _gcm_name, _gcm_cipher, _raw_cipher, _raw_context,	\
		    _blocksize )					\
static_assert ( _blocksize == sizeof ( union gcm_block ) );		\
struct cipher_algorithm _gcm_cipher = {					\
	.name		= #_gcm_name,					\
	.ctxsize	= ( sizeof ( struct gcm_context ) +		\
			    sizeof ( _raw_context ) ),			\
	.blocksize	= 1,						\
	.alignsize	= sizeof ( union gcm_block ),			\
	.authsize	= sizeof ( union gcm_block ),			\
	.setkey		= gcm_setkey,					\
	.setiv		= gcm_setiv,					\
	.encrypt	= gcm_encrypt,					\
	.decrypt	= gcm_decrypt,					\
	.auth		= gcm_auth,					\
	.priv		= &_raw_cipher,					\
};

#endif /* _IPXE_GCM_H */
