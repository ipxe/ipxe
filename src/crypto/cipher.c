#include <stdint.h>
#include <errno.h>
#include <gpxe/crypto.h>

int cipher_encrypt ( struct cipher_algorithm *cipher,
		     void *ctx, const void *src, void *dst,
		     size_t len ) {
	if ( ( len & ( cipher->blocksize - 1 ) ) ) {
		return -EINVAL;
	}
	cipher->encrypt ( ctx, src, dst, len );
	return 0;
}

int cipher_decrypt ( struct cipher_algorithm *cipher,
		     void *ctx, const void *src, void *dst,
		     size_t len ) {
	if ( ( len & ( cipher->blocksize - 1 ) ) ) {
		return -EINVAL;
	}
	cipher->decrypt ( ctx, src, dst, len );
	return 0;
}

