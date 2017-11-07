#ifndef _IPXE_MD4_H
#define _IPXE_MD4_H

/** @file
 *
 * MD4 algorithm
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/crypto.h>

/** An MD4 digest */
struct md4_digest {
	/** Hash output */
	uint32_t h[4];
};

/** An MD4 data block */
union md4_block {
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

/** MD4 digest and data block
 *
 * The order of fields within this structure is designed to minimise
 * code size.
 */
struct md4_digest_data {
	/** Digest of data already processed */
	struct md4_digest digest;
	/** Accumulated data */
	union md4_block data;
} __attribute__ (( packed ));

/** MD4 digest and data block */
union md4_digest_data_dwords {
	/** Digest and data block */
	struct md4_digest_data dd;
	/** Raw dwords */
	uint32_t dword[ sizeof ( struct md4_digest_data ) /
			sizeof ( uint32_t ) ];
};

/** An MD4 context */
struct md4_context {
	/** Amount of accumulated data */
	size_t len;
	/** Digest and accumulated data */
	union md4_digest_data_dwords ddd;
} __attribute__ (( packed ));

/** MD4 context size */
#define MD4_CTX_SIZE sizeof ( struct md4_context )

/** MD4 digest size */
#define MD4_DIGEST_SIZE sizeof ( struct md4_digest )

extern struct digest_algorithm md4_algorithm;

#endif /* _IPXE_MD4_H */
