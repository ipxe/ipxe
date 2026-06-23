#ifndef _IPXE_SHA256_H
#define _IPXE_SHA256_H

/** @file
 *
 * SHA-256 algorithm
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>
#include <endian.h>
#include <ipxe/crypto.h>
#include <ipxe/mdhash.h>

/** SHA-256 number of rounds */
#define SHA256_ROUNDS 64

/** An SHA-256 digest */
struct sha256_digest {
	/** Hash output */
	uint32_t h[8];
};

/** An SHA-256 data block */
union sha256_block {
	/** Raw bytes */
	uint8_t byte[64];
	/** Raw dwords */
	uint32_t dword[16];
	/** Final block structure */
	struct {
		/** Padding */
		uint8_t pad[56];
		/** Length in bits */
		uint64_t len;
	} final;
};

/** SHA-256 digest and data block
 *
 * The order of fields within this structure is designed to minimise
 * code size.
 */
struct sha256_digest_data {
	/** Digest of data already processed */
	struct sha256_digest digest;
	/** Accumulated data */
	union sha256_block data;
} __attribute__ (( packed ));

/** SHA-256 context size */
#define SHA256_CTX_SIZE MDHASH_CTX_SIZE ( struct sha256_digest_data )

/** SHA-256 block size */
#define SHA256_BLOCK_SIZE sizeof ( union sha256_block )

/** SHA-256 digest size */
#define SHA256_DIGEST_SIZE sizeof ( struct sha256_digest )

/** SHA-224 digest size */
#define SHA224_DIGEST_SIZE ( SHA256_DIGEST_SIZE * 224 / 256 )

extern void sha256_compress ( struct sha256_digest_data *dd,
			      const struct sha256_digest *digest );

/** Define a SHA-256 family digest algorithm */
#define SHA256_ALGORITHM( _name, _digest, _init, _digestsize )		\
	MDHASH_ALGORITHM( _name, _digest, sha256_compress,		\
			  __BIG_ENDIAN, struct sha256_digest_data,	\
			  _init, _digestsize )

extern struct digest_algorithm sha256_algorithm;
extern struct digest_algorithm sha224_algorithm;

#endif /* _IPXE_SHA256_H */
