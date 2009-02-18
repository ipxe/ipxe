#include "crypto/axtls/crypto.h"
#include <string.h>
#include <errno.h>
#include <gpxe/crypto.h>
#include <gpxe/aes.h>

struct aes_cbc_context {
	AES_CTX ctx;
	int decrypting;
};

static int aes_cbc_setkey ( void *ctx, const void *key, size_t keylen ) {
	struct aes_cbc_context *aesctx = ctx;
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

	AES_set_key ( &aesctx->ctx, key, aesctx->ctx.iv, mode );

	aesctx->decrypting = 0;

	return 0;
}

static void aes_cbc_setiv ( void *ctx, const void *iv ) {
	struct aes_cbc_context *aesctx = ctx;

	memcpy ( aesctx->ctx.iv, iv, sizeof ( aesctx->ctx.iv ) );
}

static void aes_cbc_encrypt ( void *ctx, const void *data, void *dst,
			      size_t len ) {
	struct aes_cbc_context *aesctx = ctx;

	if ( aesctx->decrypting )
		assert ( 0 );

	AES_cbc_encrypt ( &aesctx->ctx, data, dst, len );
}

static void aes_cbc_decrypt ( void *ctx, const void *data, void *dst,
			      size_t len ) {
	struct aes_cbc_context *aesctx = ctx;

	if ( ! aesctx->decrypting ) {
		AES_convert_key ( &aesctx->ctx );
		aesctx->decrypting = 1;
	}

	AES_cbc_decrypt ( &aesctx->ctx, data, dst, len );
}

struct cipher_algorithm aes_cbc_algorithm = {
	.name		= "aes_cbc",
	.ctxsize	= sizeof ( struct aes_cbc_context ),
	.blocksize	= 16,
	.setkey		= aes_cbc_setkey,
	.setiv		= aes_cbc_setiv,
	.encrypt	= aes_cbc_encrypt,
	.decrypt	= aes_cbc_decrypt,
};
