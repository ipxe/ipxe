#ifndef _IPXE_TLSKEY_H
#define _IPXE_TLSKEY_H

/** @file
 *
 * TLS key schedules
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>
#include <ipxe/crypto.h>

struct tls_phase;

/** A TLS endpoint */
struct tls_endpoint {
	/** Endpoint index */
	uint8_t index;
	/** Name (for key expansion labels) */
	const char name[ 7 /* "[client|server]" + NUL */ ];
} __attribute__ (( packed ));

/** Client endpoint index */
#define TLS_CLIENT 0

/** Server endpoint index */
#define TLS_SERVER 1

/** Number of random bytes */
#define TLS_RANDOM_LEN 32

/** TLS client or server random bytes */
struct tls_random {
	/** Random bytes */
	uint8_t bytes[TLS_RANDOM_LEN];
} __attribute__ (( packed ));

/** A TLS running handshake transcript digest */
struct tls_transcript {
	/** Digest context */
	void *ctx;
	/**
	 * Running digest value
	 *
	 * This is continuously updated to represent the digest value
	 * over the transcript up to and including the most recently
	 * digested handshake record.
	 */
	void *running;
	/**
	 * Running digest value up to the most recent Finished (if any)
	 *
	 * If no Finished record has been digested, then this mirrors
	 * the continuously running digest value.
	 *
	 * If any Finished record has been digested, then this
	 * represents the digest value over the transcript up to and
	 * including the most recently digested Finished record.
	 */
	void *finishing;
	/** Transcript flags */
	unsigned int flags;
};

/** Transcript digest includes a Hello record containing the local nonce */
#define TLSKEY_TSF_NONCED 0x0001

/** Transcript digest includes a Finished record */
#define TLSKEY_TSF_FINISHED 0x0002

/** A TLS key derivation function secret */
struct tls_kdf_secret {
	/** Secret */
	void *secret;
	/** Key flags */
	unsigned int flags;
};

/** Key derivation function has key material */
#define TLSKEY_KDF_KEYED 0x0001

/** Key derivation function has a master secret */
#define TLSKEY_KDF_MASTER 0x0002

/** Key derivation function has an extended master secret */
#define TLSKEY_KDF_EMS 0x0004

/** A TLS traffic secret */
struct tls_traffic_secret {
	/** Secret */
	void *secret;
	/** Phase */
	const struct tls_phase *phase;
};

/** A TLS key schedule */
struct tls_key_schedule {
	/** Key schedule operations */
	const struct tls_key_schedule_operations *op;
	/** Digest algorithm */
	struct digest_algorithm *digest;
	/** Secret size */
	size_t secretsize;

	/** Dynamically-allocated storage */
	void *dynamic;
	/** Secret(s) */
	void *secret;
	/** Flags */
	unsigned int flags;

	/** Running handshake transcript digest */
	struct tls_transcript transcript;
	/** Local endpoint random bytes */
	struct tls_random nonce;
	/** Client and server random bytes */
	struct tls_random random[2];
	/** Key derivation function secret */
	struct tls_kdf_secret kdf;
	/** Traffic secrets */
	struct tls_traffic_secret traffic[2];
};

/** A TLS pre-shared key */
struct tls_preshared_key {
	/** Key schedule operations (if set) */
	const struct tls_key_schedule_operations *op;
	/** Digest algorithm (if set) */
	struct digest_algorithm *digest;
	/** Key flags */
	unsigned int flags;
	/** Key material */
	union {
		/**
		 * Master secret
		 *
		 * For TLSv1.2 and earlier, session resumption works
		 * by simply copying the 48-byte raw master secret
		 * value.
		 *
		 * For TLSv1.1 and earlier, this 48-byte master secret
		 * logically comprises two halves: 24 bytes for MD5
		 * and 24 bytes for SHA-1.
		 */
		union {
			uint8_t raw[48];
			struct {
				uint8_t md5[24];
				uint8_t sha1[24];
			} __attribute__ (( packed ));
		} master_secret;
		/**
		 * Resumption secret
		 *
		 * For TLSv1.3 and later, session resumption uses a
		 * pre-shared resumption secret value generated using
		 * the HKDF Derive-Secret function.
		 *
		 * The longest possible value for any supported HKDF
		 * digest algorithm is 48 bytes, when using SHA-384.
		 */
		uint8_t resumption[48];
	} key;
};

/** TLS key schedule operations */
struct tls_key_schedule_operations {
	/** Name */
	const char *name;
	/**
	 * Key schedule accumulates all shared secrets
	 *
	 * The ability to demonstrate possession of the key material
	 * is sufficient to demonstrate possession of the most
	 * recently applied shared secret.
	 *
	 * This flag indicates that the ability to demonstrate
	 * possession of the key material is also sufficient to
	 * demonstrate possession of all previously applied shared
	 * secrets.
	 */
	int accumulates;
	/** Key flags preserved when loading key material */
	unsigned int mask;
	/**
	 * Calculate secret size
	 *
	 * @v digest		Digest algorithm
	 * @ret secretsize	Secret size, or zero if unsupported
	 */
	size_t ( * secretsize ) ( struct digest_algorithm *digest );
	/**
	 * Expand key material
	 *
	 * @v digest		Digest algorithm
	 * @v secret		Secret
	 * @v label		Label string
	 * @v seed		Seed material
	 * @v seed_len		Length of seed material
	 * @v out		Output buffer
	 * @v out_len		Length of output buffer
	 */
	void ( * expand ) ( struct digest_algorithm *digest,
			    const void *secret, const char *label,
			    const void *seed, size_t seed_len,
			    void *out, size_t out_len );
	/**
	 * Reset key schedule
	 *
	 * @v tlskey		Key schedule
	 */
	void ( * reset ) ( struct tls_key_schedule *tlskey );
	/**
	 * Apply a new shared secret
	 *
	 * @v tlskey		Key schedule
	 * @v shared		New shared secret
	 * @v shared_len	Length of new shared secret
	 * @ret rc		Return status code
	 */
	int ( * apply ) ( struct tls_key_schedule *tlskey,
			  const void *shared, size_t shared_len );
	/**
	 * Generate master secret
	 *
	 * @v tlskey		Key schedule
	 * @v ems		Extended master secret extension is enabled
	 * @ret rc		Return status code
	 */
	int ( * master ) ( struct tls_key_schedule *tlskey, int ems );
	/**
	 * Generate verification data
	 *
	 * @v tlskey		Key schedule
	 * @v end		Verification endpoint
	 * @v verify		Verification data
	 * @v verify_len	Length of verification data
	 * @ret rc		Return status code
	 */
	int ( * verify ) ( struct tls_key_schedule *tlskey,
			   const struct tls_endpoint *end,
			   void *verify, size_t verify_len );
	/**
	 * Generate traffic secrets
	 *
	 * @v tlskey		Key schedule
	 * @v writer		Writer endpoint
	 * @v phase		Traffic phase
	 * @ret rc		Return status code
	 */
	int ( * traffic ) ( struct tls_key_schedule *tlskey,
			    const struct tls_endpoint *writer,
			    const struct tls_phase *phase );
	/**
	 * Generate cipher key material
	 *
	 * @v tlskey		Key schedule
	 * @v writer		Writer endpoint
	 * @v key		Cipher key to fill in
	 * @v key_len		Length of cipher key
	 * @v iv		Fixed portion of IV to fill in
	 * @v iv_len		Length of fixed portion of IV
	 * @v mac		MAC secret to fill in
	 * @v mac_len		Length of MAC secret
	 * @ret rc		Return status code
	 */
	int ( * cipher ) ( struct tls_key_schedule *tlskey,
			   const struct tls_endpoint *writer,
			   void *key, size_t key_len, void *iv,
			   size_t iv_len, void *mac, size_t mac_len );
	/**
	 * Generate signable digest value
	 *
	 * @v tlskey		Key schedule
	 * @v end		Endpoint
	 * @v digest		Signature digest algorithm
	 * @v data		Additional data
	 * @v len		Length of additional data
	 * @v tbs		Signature digest value to fill in
	 * @ret rc		Return status code
	 */
	int ( * tbshash ) ( struct tls_key_schedule *tlskey,
			    const struct tls_endpoint *end,
			    struct digest_algorithm *digest,
			    const void *data, size_t len, void *tbs );
	/**
	 * Save pre-shared key
	 *
	 * @v tlskey		Key schedule
	 * @v nonce		Ticket nonce
	 * @v nonce_len		Length of ticket nonce
	 * @v psk		Pre-shared key to fill in
	 * @ret rc		Return status code
	 */
	int ( * save ) ( struct tls_key_schedule *tlskey, const void *nonce,
			 size_t nonce_len, struct tls_preshared_key *psk );
	/**
	 * Load pre-shared key
	 *
	 * @v tlskey		Key schedule
	 * @v psk		Pre-shared key
	 * @ret rc		Return status code
	 */
	int ( * load ) ( struct tls_key_schedule *tlskey,
			 const struct tls_preshared_key *psk );
	/**
	 * Generate pre-shared key binder value
	 *
	 * @v psk		Pre-shared key
	 * @v hash		Partial transcript hash
	 * @v binder		Binder value to fill in
	 * @v binder_len	Length of binder value
	 * @ret rc		Return status code
	 */
	int ( * bind ) ( const struct tls_preshared_key *psk,
			 const void *hash, void *binder, size_t binder_len );
};

/**
 * Check if key schedule accumulates shared secrets
 *
 * @v tlskey		Key schedule
 * @ret is_accumulating	Key schedule accumulates all shared secrets
 */
static inline __attribute__ (( always_inline )) int
tlskey_is_accumulating ( struct tls_key_schedule *tlskey ) {
	const struct tls_key_schedule_operations *op = tlskey->op;

	return ( op && op->accumulates );
}

extern const struct tls_endpoint tls_client;
extern const struct tls_endpoint tls_server;
extern const struct tls_phase tls_early;
extern const struct tls_phase tls_handshake;
extern const struct tls_phase tls_application;

extern const struct tls_key_schedule_operations tlskey_hkdf;
extern const struct tls_key_schedule_operations tlskey_hash;
extern const struct tls_key_schedule_operations tlskey_md5_sha1;

extern int tlskey_start ( struct tls_key_schedule *tlskey,
			  const struct tls_key_schedule_operations *op,
			  struct digest_algorithm *digest,
			  const struct tls_random *random );
extern void tlskey_stop ( struct tls_key_schedule *tlskey );
extern void tlskey_digest ( struct tls_key_schedule *tlskey,
			    const void *data, size_t len );
extern void tlskey_reset ( struct tls_key_schedule *tlskey );
extern int tlskey_apply ( struct tls_key_schedule *tlskey,
			  const void *shared, size_t shared_len );
extern int tlskey_master ( struct tls_key_schedule *tlskey, int ems );
extern int tlskey_verify ( struct tls_key_schedule *tlskey,
			   const struct tls_endpoint *end,
			   void *verify, size_t verify_len );
extern int tlskey_traffic ( struct tls_key_schedule *tlskey,
			    const struct tls_endpoint *writer,
			    const struct tls_phase *phase );
extern int tlskey_cipher ( struct tls_key_schedule *tlskey,
			   const struct tls_endpoint *writer,
			   void *key, size_t key_len, void *iv,
			   size_t iv_len, void *mac, size_t mac_len );
extern int tlskey_tbshash ( struct tls_key_schedule *tlskey,
			    const struct tls_endpoint *end,
			    struct digest_algorithm *digest,
			    const void *data, size_t len, void *tbs );
extern int tlskey_save ( struct tls_key_schedule *tlskey, const void *nonce,
			 size_t nonce_len, struct tls_preshared_key *psk );
extern int tlskey_load ( struct tls_key_schedule *tlskey, int ems,
			 const struct tls_preshared_key *psk );
extern int tlskey_bind ( const struct tls_preshared_key *psk,
			 const void *prefix, size_t prefix_len,
			 void *binder, size_t binder_len );

#endif /* _IPXE_TLSKEY_H */
