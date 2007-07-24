#include <stdint.h>
#include <errno.h>
#include <gpxe/crypto.h>

int cipher_encrypt ( struct crypto_algorithm *crypto,
		     void *ctx, const void *src, void *dst,
		     size_t len ) {
	if ( ( len & ( crypto->blocksize - 1 ) ) ) {
		return -EINVAL;
	}
	crypto->encode ( ctx, src, dst, len );
	return 0;
}

int cipher_decrypt ( struct crypto_algorithm *crypto,
		     void *ctx, const void *src, void *dst,
		     size_t len ) {
	if ( ( len & ( crypto->blocksize - 1 ) ) ) {
		return -EINVAL;
	}
	crypto->decode ( ctx, src, dst, len );
	return 0;
}

