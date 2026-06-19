#ifndef _IPXE_CBC_H
#define _IPXE_CBC_H

/** @file
 *
 * Cipher-block chaining
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <ipxe/crypto.h>

/** A cipher-block chaining mode context */
#define cbc_context_t( blocksize, ctxsize )				\
	struct {							\
		uint8_t cbc[ (blocksize) ];				\
		uint8_t raw[ (ctxsize) - (blocksize) ];			\
	}

extern int cbc_setkey ( struct cipher_algorithm *cipher, void *ctx,
			const void *key, size_t keylen );
extern int cbc_setiv ( struct cipher_algorithm *cipher, void *ctx,
		       const void *iv, size_t ivlen );
extern void cbc_encrypt ( struct cipher_algorithm *cipher, void *ctx,
			  const void *src, void *dst, size_t len );
extern void cbc_decrypt ( struct cipher_algorithm *cipher, void *ctx,
			  const void *src, void *dst, size_t len );

/**
 * Create a cipher-block chaining mode of behaviour of an existing cipher
 *
 * @v _cbc_name		Name for the new CBC cipher
 * @v _cbc_cipher	New cipher algorithm
 * @v _raw_cipher	Underlying cipher algorithm
 * @v _raw_context	Context structure for the underlying cipher
 * @v _blocksize	Cipher block size
 */
#define CBC_CIPHER( _cbc_name, _cbc_cipher, _raw_cipher, _raw_context,	\
		    _blocksize )					\
struct cipher_algorithm _cbc_cipher = {					\
	.name		= #_cbc_name,					\
	.ctxsize	= ( _blocksize + sizeof ( _raw_context ) ),	\
	.blocksize	= _blocksize,					\
	.alignsize	= _blocksize,					\
	.authsize	= 0,						\
	.setkey		= cbc_setkey,					\
	.setiv		= cbc_setiv,					\
	.encrypt	= cbc_encrypt,					\
	.decrypt	= cbc_decrypt,					\
	.auth		= cipher_null_auth,				\
	.priv		= &_raw_cipher,					\
};

#endif /* _IPXE_CBC_H */
