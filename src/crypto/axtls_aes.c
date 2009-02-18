#include "crypto/axtls/crypto.h"
#include <string.h>
#include <errno.h>
#include <gpxe/crypto.h>
#include <gpxe/aes.h>

static int aes_cbc_setkey ( void *ctx, const void *key, size_t keylen ) {
	AES_CTX *aesctx = ctx;
	AES_MODE mode;

	switch ( keylen ) {
	case ( 128 / 8 ):
		mode = AES_MODE_128;
		break;
	case ( 256 / 8 ):
		mode = AES_MODE_256;
		break;
	default:
		return -EINVAL;
	}

	AES_set_key ( aesctx, key, aesctx->iv, mode );
	return 0;
}

static void aes_cbc_setiv ( void *ctx, const void *iv ) {
	AES_CTX *aesctx = ctx;

	memcpy ( aesctx->iv, iv, sizeof ( aesctx->iv ) );
}

static void aes_cbc_encrypt ( void *ctx, const void *data, void *dst,
			      size_t len ) {
	AES_CTX *aesctx = ctx;

	AES_cbc_encrypt ( aesctx, data, dst, len );
}

static void aes_cbc_decrypt ( void *ctx, const void *data, void *dst,
			      size_t len ) {
	AES_CTX *aesctx = ctx;

	AES_cbc_decrypt ( aesctx, data, dst, len );
}

struct crypto_algorithm aes_cbc_algorithm = {
	.name		= "aes_cbc",
	.ctxsize	= sizeof ( AES_CTX ),
	.blocksize	= 16,
	.setkey		= aes_cbc_setkey,
	.setiv		= aes_cbc_setiv,
	.encode		= aes_cbc_encrypt,
	.decode		= aes_cbc_decrypt,
};
