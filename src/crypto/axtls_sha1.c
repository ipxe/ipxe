#include "crypto/axtls/crypto.h"
#include <gpxe/crypto.h>
#include <gpxe/sha1.h>

static void sha1_init ( void *ctx ) {
	SHA1Init ( ctx );
}

static void sha1_update ( void *ctx, const void *data, void *dst __unused,
			  size_t len ) {
	SHA1Update ( ctx, data, len );
}

static void sha1_final ( void *ctx, void *out ) {
	SHA1Final ( ctx, out );
}

struct crypto_algorithm sha1_algorithm = {
	.name		= "sha1",
	.ctxsize	= SHA1_CTX_SIZE,
	.blocksize	= 64,
	.digestsize	= SHA1_DIGEST_SIZE,
	.init		= sha1_init,
	.encode		= sha1_update,
	.final		= sha1_final,
};
