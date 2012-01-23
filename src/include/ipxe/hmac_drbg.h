#ifndef _IPXE_HMAC_DRBG_H
#define _IPXE_HMAC_DRBG_H

/** @file
 *
 * HMAC_DRBG algorithm
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <ipxe/sha1.h>

/** Use SHA-1 as the underlying hash algorithm
 *
 * HMAC_DRBG using SHA-1 is an Approved algorithm in ANS X9.82.
 */
#define hmac_drbg_algorithm sha1_algorithm

/** Maximum security strength
 *
 * The maximum security strength of HMAC_DRBG using SHA-1 is 128 bits
 * (according to the list of maximum security strengths documented in
 * NIST SP 800-57 Part 1 Section 5.6.1 Table 3).
 */
#define HMAC_DRBG_MAX_SECURITY_STRENGTH 128

/** Security strength
 *
 * For the sake of implementation simplicity, only a single security
 * strength is supported, which is the maximum security strength
 * supported by the algorithm.
 */
#define HMAC_DRBG_SECURITY_STRENGTH HMAC_DRBG_MAX_SECURITY_STRENGTH

/** Underlying hash algorithm output length (in bytes) */
#define HMAC_DRBG_OUTLEN_BYTES SHA1_DIGEST_SIZE

/** Required minimum entropy for instantiate and reseed
 *
 * The minimum required entropy for HMAC_DRBG is equal to the security
 * strength according to ANS X9.82 Part 3-2007 Section 10.2.1 Table 2
 * (NIST SP 800-90 Section 10.1 Table 2).
 */
#define HMAC_DRBG_MIN_ENTROPY_BYTES ( HMAC_DRBG_SECURITY_STRENGTH / 8 )

/** Minimum entropy input length
 *
 * The minimum entropy input length for HMAC_DRBG is equal to the
 * security strength according to ANS X9.82 Part 3-2007 Section 10.2.1
 * Table 2 (NIST SP 800-90 Section 10.1 Table 2).
 */
#define HMAC_DRBG_MIN_ENTROPY_LEN_BYTES ( HMAC_DRBG_SECURITY_STRENGTH / 8 )

/** Maximum entropy input length
 *
 * The maximum entropy input length for HMAC_DRBG is 2^35 bits
 * according to ANS X9.82 Part 3-2007 Section 10.2.1 Table 2 (NIST SP
 * 800-90 Section 10.1 Table 2).
 *
 * We choose to allow up to 2^32-1 bytes (i.e. 2^35-8 bits).
 */
#define HMAC_DRBG_MAX_ENTROPY_LEN_BYTES 0xffffffffUL

/** Maximum personalisation string length
 *
 * The maximum permitted personalisation string length for HMAC_DRBG
 * is 2^35 bits according to ANS X9.82 Part 3-2007 Section 10.2.1
 * Table 1 (NIST SP 800-90 Section 10.1 Table 2).
 *
 * We choose to allow up to 2^32-1 bytes (i.e. 2^35-8 bits).
 */
#define HMAC_DRBG_MAX_PERSONAL_LEN_BYTES 0xffffffffUL

/** Maximum additional input length
 *
 * The maximum permitted additional input length for HMAC_DRBG is 2^35
 * bits according to ANS X9.82 Part 3-2007 Section 10.2.1 Table 1
 * (NIST SP 800-90 Section 10.1 Table 2).
 *
 * We choose to allow up to 2^32-1 bytes (i.e. 2^35-8 bits).
 */
#define HMAC_DRBG_MAX_ADDITIONAL_LEN_BYTES 0xffffffffUL

/** Maximum length of generated pseudorandom data per request
 *
 * The maximum number of bits per request for HMAC_DRBG is 2^19 bits
 * according to ANS X9.82 Part 3-2007 Section 10.2.1 Table 1 (NIST SP
 * 800-90 Section 10.1 Table 2).
 *
 * We choose to allow up to 2^16-1 bytes (i.e. 2^19-8 bits).
 */
#define HMAC_DRBG_MAX_GENERATED_LEN_BYTES 0x0000ffffUL

/** Reseed interval
 *
 * The maximum permitted reseed interval for HMAC_DRBG using SHA-1 is
 * 2^48 according to ANS X9.82 Part 3-2007 Section 10.2.1 Table 2
 * (NIST SP 800-90 Section 10.1 Table 2).  However, the sample
 * implementation given in ANS X9.82 Part 3-2007 Annex E.2.1 (NIST SP
 * 800-90 Appendix F.2) shows a reseed interval of 10000.
 *
 * We choose a very conservative reseed interval.
 */
#define HMAC_DRBG_RESEED_INTERVAL 1024

/** Underlying hash algorithm context size (in bytes) */
#define HMAC_DRBG_CTX_SIZE SHA1_CTX_SIZE

/**
 * HMAC_DRBG internal state
 *
 * This structure is defined by ANS X9.82 Part 3-2007 Section
 * 10.2.2.2.1 (NIST SP 800-90 Section 10.1.2.1).
 *
 * The "administrative information" portions (security_strength and
 * prediction_resistance) are design-time constants and so are not
 * present as fields in this structure.
 */
struct hmac_drbg_state {
	/** Current value
	 *
	 * "The value V of outlen bits, which is updated each time
	 * another outlen bits of output are produced"
	 */
	uint8_t value[HMAC_DRBG_OUTLEN_BYTES];
	/** Current key
	 *
	 * "The outlen-bit Key, which is updated at least once each
	 * time that the DRBG mechanism generates pseudorandom bits."
	 */
	uint8_t key[HMAC_DRBG_OUTLEN_BYTES];
	/** Reseed counter
	 *
	 * "A counter (reseed_counter) that indicates the number of
	 * requests for pseudorandom bits since instantiation or
	 * reseeding"
	 */
	unsigned int reseed_counter;
};

extern void hmac_drbg_instantiate ( struct hmac_drbg_state *state,
				    const void *entropy, size_t entropy_len,
				    const void *personal, size_t personal_len );
extern void hmac_drbg_reseed ( struct hmac_drbg_state *state,
			       const void *entropy, size_t entropy_len,
			       const void *additional, size_t additional_len );
extern int hmac_drbg_generate ( struct hmac_drbg_state *state,
				const void *additional, size_t additional_len,
				void *data, size_t len );

#endif /* _IPXE_HMAC_DRBG_H */
