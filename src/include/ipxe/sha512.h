#ifndef _IPXE_SHA512_H
#define _IPXE_SHA512_H

/** @file
 *
 * SHA-512 algorithm
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>
#include <endian.h>
#include <ipxe/crypto.h>
#include <ipxe/mdhash.h>

/** SHA-512 number of rounds */
#define SHA512_ROUNDS 80

/** An SHA-512 digest */
struct sha512_digest {
	/** Hash output */
	uint64_t h[8];
};

/** An SHA-512 data block */
union sha512_block {
	/** Raw bytes */
	uint8_t byte[128];
	/** Raw qwords */
	uint64_t qword[16];
	/** Final block structure */
	struct {
		/** Padding */
		uint8_t pad[112];
		/** Length in bits */
		struct {
			/** High 64 bits (unused by iPXE) */
			uint64_t hi;
			/** Low 64 bits (unused by iPXE) */
			uint64_t lo;
		} __attribute__ (( packed )) len;
	} final;
};

/** SHA-512 digest and data block
 *
 * The order of fields within this structure is designed to minimise
 * code size.
 */
struct sha512_digest_data {
	/** Digest of data already processed */
	struct sha512_digest digest;
	/** Accumulated data */
	union sha512_block data;
} __attribute__ (( packed ));

/** SHA-512 context size */
#define SHA512_CTX_SIZE MDHASH_CTX_SIZE ( struct sha512_digest_data )

/** SHA-512 block size */
#define SHA512_BLOCK_SIZE sizeof ( union sha512_block )

/** SHA-512 digest size */
#define SHA512_DIGEST_SIZE sizeof ( struct sha512_digest )

/** SHA-384 digest size */
#define SHA384_DIGEST_SIZE ( SHA512_DIGEST_SIZE * 384 / 512 )

/** SHA-512/256 digest size */
#define SHA512_256_DIGEST_SIZE ( SHA512_DIGEST_SIZE * 256 / 512 )

/** SHA-512/224 digest size */
#define SHA512_224_DIGEST_SIZE ( SHA512_DIGEST_SIZE * 224 / 512 )

extern void sha512_compress ( struct sha512_digest_data *dd,
			      const struct sha512_digest *digest );

/** Define a SHA-512 family digest algorithm */
#define SHA512_ALGORITHM( _name, _digest, _init, _digestsize )		\
	MDHASH_ALGORITHM( _name, _digest, sha512_compress,		\
			  __BIG_ENDIAN, struct sha512_digest_data,	\
			  _init, _digestsize )

extern struct digest_algorithm sha512_algorithm;
extern struct digest_algorithm sha384_algorithm;
extern struct digest_algorithm sha512_256_algorithm;
extern struct digest_algorithm sha512_224_algorithm;

#endif /* IPXE_SHA512_H */
