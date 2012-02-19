#ifndef _IPXE_HASH_DF_H
#define _IPXE_HASH_DF_H

/** @file
 *
 * Hash-based derivation function (Hash_df)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <ipxe/sha1.h>

/** Use SHA-1 as the underlying hash algorithm
 *
 * Hash_df using SHA-1 is an Approved algorithm in ANS X9.82.
 */
#define hash_df_algorithm sha1_algorithm

/** Underlying hash algorithm output length (in bytes) */
#define HASH_DF_OUTLEN_BYTES SHA1_DIGEST_SIZE

/** Underlying hash algorithm context size (in bytes) */
#define HASH_DF_CTX_SIZE SHA1_CTX_SIZE

extern void hash_df ( const void *input, size_t input_len, void *output,
		      size_t output_len );

#endif /* _IPXE_HASH_DF_H */
