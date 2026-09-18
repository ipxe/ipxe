#ifndef _IPXE_MD5_SHA1_H
#define _IPXE_MD5_SHA1_H

/** @file
 *
 * Hybrid MD5+SHA1 hash as used by TLSv1.1 and earlier
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>
#include <ipxe/crypto.h>
#include <ipxe/md5.h>
#include <ipxe/sha1.h>

/** An MD5+SHA1 context */
struct md5_sha1_context {
	/** MD5 context */
	uint8_t md5[MD5_CTX_SIZE];
	/** SHA-1 context */
	uint8_t sha1[SHA1_CTX_SIZE];
} __attribute__ (( packed ));

/** MD5+SHA1 context size */
#define MD5_SHA1_CTX_SIZE sizeof ( struct md5_sha1_context )

/** An MD5+SHA1 digest */
struct md5_sha1_digest {
	/** MD5 digest */
	uint8_t md5[MD5_DIGEST_SIZE];
	/** SHA-1 digest */
	uint8_t sha1[SHA1_DIGEST_SIZE];
} __attribute__ (( packed ));

/** MD5+SHA1 digest size */
#define MD5_SHA1_DIGEST_SIZE sizeof ( struct md5_sha1_digest )

/** An MD5+SHA1 data block */
union md5_sha1_block {
	/** MD5 data block */
	uint8_t md5[MD5_BLOCK_SIZE];
	/** SHA-1 data block */
	uint8_t sha1[SHA1_BLOCK_SIZE];
};

/** MD5+SHA1 block size */
#define MD5_SHA1_BLOCK_SIZE sizeof ( union md5_sha1_block )

/** An MD5+SHA1 HMAC key block
 *
 * The hybrid PRF used in TLS version 1.1 and earlier does not use
 * HMAC with the hybrid MD5+SHA1 algorithm: it uses separate
 * invocations of HMAC-MD5 and HMAC-SHA1.
 */
struct md5_sha1_hmac_keys {
	/** MD5 HMAC key */
	uint8_t md5[MD5_BLOCK_SIZE];
	/** SHA-1 HMAC key */
	uint8_t sha1[SHA1_BLOCK_SIZE];
};

extern char md5_sha1_sentinel[1];
extern struct digest_algorithm md5_sha1_algorithm;

/**
 * Check if a digest algorithm is MD5+SHA1
 *
 * @v digest		Digest algorithm
 * @v is_md5_sha1	Digest algorithm is MD5+SHA1
 */
static inline __attribute__ (( always_inline )) int
is_md5_sha1 ( struct digest_algorithm *digest ) {

	/* The MD5+SHA1 algorithm is hardcoded into various parts of
	 * the TLS specifications prior to TLSv1.2, which has knock-on
	 * effects on non-TLS code such as the RSA algorithm (which
	 * needs to be able to identify the PKCS#1 digestInfo prefix).
	 *
	 * Allow a digest algorithm to be tested to see if it is the
	 * MD5+SHA1 algorithm without creating an explicit symbol
	 * reference that will drag the algorithm into a build that
	 * otherwise does not need it.
	 */
	return ( digest->priv == md5_sha1_sentinel );
}

#endif /* _IPXE_MD5_SHA1_H */
