#ifndef _IPXE_GCM_H
#define _IPXE_GCM_H

/** @file
 *
 * Galois/Counter Mode (GCM)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

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
	/** Underlying block cipher */
	struct cipher_algorithm *raw_cipher;
	/** Underlying block cipher context */
	uint8_t raw_ctx[0];
};

extern void gcm_tag ( struct gcm_context *context, union gcm_block *tag );
extern int gcm_setkey ( struct gcm_context *context, const void *key,
			size_t keylen, struct cipher_algorithm *raw_cipher );
extern void gcm_setiv ( struct gcm_context *context, const void *iv,
			size_t ivlen );
extern void gcm_encrypt ( struct gcm_context *context, const void *src,
			  void *dst, size_t len );
extern void gcm_decrypt ( struct gcm_context *context, const void *src,
			  void *dst, size_t len );

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
struct _gcm_name ## _context {						\
	/** GCM context */						\
	struct gcm_context gcm;						\
	/** Underlying block cipher context */				\
	_raw_context raw;						\
};									\
static int _gcm_name ## _setkey ( void *ctx, const void *key,		\
				  size_t keylen ) {			\
	struct _gcm_name ## _context *context = ctx;			\
	build_assert ( _blocksize == sizeof ( context->gcm.key ) );	\
	build_assert ( offsetof ( typeof ( *context ), gcm ) == 0 );	\
	build_assert ( offsetof ( typeof ( *context ), raw ) ==		\
		       offsetof ( typeof ( *context ), gcm.raw_ctx ) );	\
	return gcm_setkey ( &context->gcm, key, keylen, &_raw_cipher );	\
}									\
static void _gcm_name ## _setiv ( void *ctx, const void *iv,		\
				  size_t ivlen ) {			\
	struct _gcm_name ## _context *context = ctx;			\
	gcm_setiv ( &context->gcm, iv, ivlen );				\
}									\
static void _gcm_name ## _encrypt ( void *ctx, const void *src,		\
				    void *dst, size_t len ) {		\
	struct _gcm_name ## _context *context = ctx;			\
	gcm_encrypt ( &context->gcm, src, dst, len );			\
}									\
static void _gcm_name ## _decrypt ( void *ctx, const void *src,		\
				    void *dst, size_t len ) {		\
	struct _gcm_name ## _context *context = ctx;			\
	gcm_decrypt ( &context->gcm, src, dst, len );			\
}									\
static void _gcm_name ## _auth ( void *ctx, void *auth ) {		\
	struct _gcm_name ## _context *context = ctx;			\
	union gcm_block *tag = auth;					\
	gcm_tag ( &context->gcm, tag );					\
}									\
struct cipher_algorithm _gcm_cipher = {					\
	.name		= #_gcm_name,					\
	.ctxsize	= sizeof ( struct _gcm_name ## _context ),	\
	.blocksize	= 1,						\
	.alignsize	= sizeof ( union gcm_block ),			\
	.authsize	= sizeof ( union gcm_block ),			\
	.setkey		= _gcm_name ## _setkey,				\
	.setiv		= _gcm_name ## _setiv,				\
	.encrypt	= _gcm_name ## _encrypt,			\
	.decrypt	= _gcm_name ## _decrypt,			\
	.auth		= _gcm_name ## _auth,				\
};

#endif /* _IPXE_GCM_H */
