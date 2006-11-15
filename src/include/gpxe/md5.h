#ifndef _GPXE_MD5_H
#define _GPXE_MD5_H

#include <stdint.h>

#define MD5_DIGEST_SIZE		16
#define MD5_BLOCK_WORDS		16
#define MD5_HASH_WORDS		4

struct md5_context {
	u32 hash[MD5_HASH_WORDS];
	u32 block[MD5_BLOCK_WORDS];
	u64 byte_count;
};

struct md5_hash {
	u8 hash[MD5_DIGEST_SIZE];
};

extern void md5_init ( struct md5_context *context );
extern void md5_update ( struct md5_context *context, const void *data,
			 size_t len );
extern void md5_finish ( struct md5_context *context, struct md5_hash *hash );

#endif /* _GPXE_MD5_H */
