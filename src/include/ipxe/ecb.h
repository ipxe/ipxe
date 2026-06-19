#ifndef _IPXE_ECB_H
#define _IPXE_ECB_H

/** @file
 *
 * Electronic codebook (ECB)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <ipxe/crypto.h>

extern int ecb_setkey ( struct cipher_algorithm *cipher, void *ctx,
			const void *key, size_t keylen );
extern void ecb_encrypt ( struct cipher_algorithm *cipher, void *ctx,
			  const void *src, void *dst, size_t len );
extern void ecb_decrypt ( struct cipher_algorithm *cipher, void *ctx,
			  const void *src, void *dst, size_t len );

/**
 * Create an electronic codebook mode of behaviour of an existing cipher
 *
 * @v _ecb_name		Name for the new ECB cipher
 * @v _ecb_cipher	New cipher algorithm
 * @v _raw_cipher	Underlying cipher algorithm
 * @v _raw_context	Context structure for the underlying cipher
 * @v _blocksize	Cipher block size
 */
#define ECB_CIPHER( _ecb_name, _ecb_cipher, _raw_cipher, _raw_context,	\
		    _blocksize )					\
struct cipher_algorithm _ecb_cipher = {					\
	.name		= #_ecb_name,					\
	.ctxsize	= sizeof ( _raw_context ),			\
	.blocksize	= _blocksize,					\
	.alignsize	= _blocksize,					\
	.authsize	= 0,						\
	.setkey		= ecb_setkey,					\
	.setiv		= cipher_null_setiv,				\
	.encrypt	= ecb_encrypt,					\
	.decrypt	= ecb_decrypt,					\
	.auth		= cipher_null_auth,				\
	.priv		= &_raw_cipher,					\
};

#endif /* _IPXE_ECB_H */
