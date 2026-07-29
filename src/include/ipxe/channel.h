#ifndef _IPXE_CHANNEL_H
#define _IPXE_CHANNEL_H

/** @file
 *
 * Secure channel abstraction
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>
#include <ipxe/crypto.h>
#include <ipxe/x509.h>
#include <ipxe/sha256.h>

/** Ephemeral master secret digest algorithm */
#define channel_ephemeral_algorithm sha256_algorithm

/** Secure channel properties */
struct secure_channel_properties {
	/** Channel contains key material derived from a shared secret */
	int keyed;
	/**
	 * Bound peer identity (if any)
	 *
	 * This is guaranteed to be set only if the "keyed" property
	 * is also set.
	 */
	struct x509_certificate *bound;
	/**
	 * Confirmed peer identity (if any)
	 *
	 * This is guaranteed to be set only if the "bound" property
	 * is also set, and then to have the same value as the "bound"
	 * property.
	 */
	struct x509_certificate *confirmed;
	/**
	 * Established peer identity (if any)
	 *
	 * This is guaranteed to be set only if the "confirmed"
	 * property is also set, and then to have the same value as
	 * both the "confirmed" and "bound" properties.
	 *
	 * If an established peer identity exists, then it is
	 * guaranteed that the channel may be used for confidential
	 * communication with the peer that was specified at the point
	 * that the channel was established as trusted.
	 */
	struct x509_certificate *established;
};

/** A secure channel transmit or receive pipe */
struct secure_pipe {
	/** Cipher algorithm */
	struct cipher_algorithm *cipher;
	/** Cipher context */
	void *ctx;
};

/**
 * A secure channel
 *
 * This is designed to be embedded within a containing structure that
 * represents the consumer, such as a TLS connection.
 *
 * The containing structure must have been initialised to all zeros
 * before use.
 */
struct secure_channel {
	/** Channel operations */
	struct secure_channel_operations *op;

	/** Ephemeral master secret */
	uint8_t ephemeral[SHA256_DIGEST_SIZE];
	/** Security properties */
	struct secure_channel_properties props;

	/** Transmit pipe */
	struct secure_pipe tx;
	/** Receive pipe */
	struct secure_pipe rx;
};

/**
 * A pre-shared bound peer identity
 *
 * This is designed to be embedded within a containing structure that
 * also holds the pre-shared key material, such as a TLS session
 * structure.
 *
 * The containing structure must have been initialised to all zeros
 * before use.
 */
struct secure_preshared_identity {
	/** Bound peer identity */
	struct x509_certificate *bound;
};

/** Secure channel operations */
struct secure_channel_operations {
	/**
	 * Reset the key schedule
	 *
	 * @v channel		Secure channel
	 *
	 * Return the key schedule to a freshly initialised state,
	 * with all key material destroyed.
	 *
	 * The consumer must leave the key schedule in a state that is
	 * ready to receive an initial shared secret.
	 *
	 * This method is used to clean up after errors and when the
	 * secure channel is closed.  It may be invoked repeatedly,
	 * and may not fail.
	 */
	void ( * reset ) ( struct secure_channel *channel );
	/**
	 * Apply a new shared secret to key schedule
	 *
	 * @v channel		Secure channel
	 * @v exchange		Key exchange algorithm
	 * @v shared		New shared secret
	 * @v accumulated	Accumulation flag to fill in
	 * @ret rc		Return status code
	 *
	 * Incorporate a new shared secret into the key schedule to
	 * produce the key material required to operate the channel
	 * (e.g. to subsequently derive traffic keys).
	 *
	 * If the resulting key material is cryptographically
	 * dependent upon all previously incorporated shared secrets
	 * as well as upon the new shared secret (as is the case with
	 * the HKDF-based key schedule used in TLS version 1.3), then
	 * this method may set the accumulation flag.  If the key
	 * material depends solely upon the new shared secret (as is
	 * the case with the master secret computation in earlier
	 * versions of TLS), then the method must not set the
	 * accumulation flag.
	 *
	 * This method implicitly asserts to the secure channel that
	 * the ability of the peer to subsequently demonstrate
	 * possession of the resulting key material is sufficient to
	 * demonstrate the peer's possession of the new shared secret.
	 *
	 * Setting the accumulation flag asserts to the secure channel
	 * that the ability of the peer to demonstrate possession of
	 * the resulting key material is also sufficient to
	 * demonstrate the peer's possession of all previously
	 * incorporated shared secrets, and therefore that any peer
	 * identity that was already bound to the old shared secret
	 * remains bound to the new shared secret.
	 *
	 * Conversely, leaving the accumulation flag unset indicates
	 * to the secure channel that the ability of the peer to
	 * demonstrate possession of the resulting key material does
	 * not necessarily suffice to demonstrate possession of any
	 * previously incorporated shared secrets, and therefore that
	 * any peer identity that was bound to the old shared secret
	 * is not bound to the new shared secret.
	 *
	 * If this method does not set the accumulation flag, then the
	 * secure channel will therefore automatically unbind any
	 * stored peer identity that had previously been bound to the
	 * old shared secret.
	 *
	 * If this method returns an error, then the secure channel
	 * will be reset to a non-keyed state.
	 */
	int ( * apply ) ( struct secure_channel *channel,
			  struct exchange_algorithm *exchange,
			  const void *shared, int *accumulated );
	/**
	 * Save a pre-shared key for future resumption of the key schedule
	 *
	 * @v channel		Secure channel
	 * @v psid		Pre-shared bound peer identity
	 * @ret rc		Return status code
	 *
	 * Save key material alongside an already bound peer identity,
	 * so that a future operation may resume the key schedule with
	 * the peer identity already bound to the key material.
	 *
	 * This method asserts to the secure channel that the ability
	 * of the peer to subsequently demonstrate possession of the
	 * future resumed key material is sufficient to demonstrate
	 * the peer's possession of the current key material.
	 *
	 * This method is optional if the consumer does not support
	 * resumption from a pre-shared key.
	 *
	 * If this method returns an error, then any existing
	 * pre-shared bound peer identity will be cleared.
	 */
	int ( * save ) ( struct secure_channel *channel,
			 struct secure_preshared_identity *psid );
	/**
	 * Load a pre-shared key and resume the key schedule
	 *
	 * @v channel		Secure channel
	 * @v psid		Pre-shared bound peer identity
	 * @ret rc		Return status code
	 *
	 * Load key material that was previously saved alongside a
	 * bound peer identity.
	 *
	 * This method asserts to the secure channel that the ability
	 * of the peer to subsequently demonstrate possession of the
	 * resumed key material is sufficient to demonstrate the
	 * peer's possession of the originally saved key material.
	 *
	 * This method is optional if the consumer does not support
	 * resumption from a pre-shared key.
	 *
	 * If this method returns an error, then the secure channel
	 * will be reset to a non-keyed state.
	 */
	int ( * load ) ( struct secure_channel *channel,
			 struct secure_preshared_identity *psid );
	/**
	 * Verify authenticator value
	 *
	 * @v channel		Secure channel
	 * @v auth		Authenticator value
	 * @v len		Length of authenticator value
	 * @ret rc		Return status code
	 *
	 * Verify that the peer has demonstrated possession of the key
	 * material (and hence has demonstrated possession of the
	 * identity-bound shared secret).
	 *
	 * This method asserts to the secure channel that the
	 * authenticator value is guaranteed to be non-replayable
	 * (e.g. by being constructed as a digest that covers a
	 * challenge nonce, as with the TLS Finished verification data
	 * which covers the client random bytes within the handshake
	 * transcript).
	 *
	 * If this method returns an error, then the secure channel
	 * will be left in a non-confirmed state.
	 */
	int ( * verify ) ( struct secure_channel *channel,
			   const void *auth, size_t len );
};

/**
 * Initialise secure channel
 *
 * @v channel		Secure channel
 * @v op		Channel operations
 */
static inline void channel_init ( struct secure_channel *channel,
				  struct secure_channel_operations *op ) {

	channel->op = op;
}

/**
 * Check if secure channel has been established
 *
 * @v channel		Secure channel
 * @ret is_established	Channel has been established
 */
static inline __attribute__ (( always_inline )) int
channel_is_established ( struct secure_channel *channel ) {

	return ( channel->props.established != NULL );
}

/**
 * Clear pre-shared bound peer identity
 *
 * @v psid		Pre-shared identity
 */
static inline __attribute__ (( always_inline )) void
channel_clear_preshared ( struct secure_preshared_identity *psid ) {

	x509_put ( psid->bound );
	psid->bound = NULL;
}

extern void channel_ephemeral ( struct secure_channel *channel,
				const void *info, size_t info_len,
				void *out, size_t len );
extern void channel_ephemeral_label ( struct secure_channel *channel,
				      const char *label, void *out,
				      size_t len );
extern void channel_unkey ( struct secure_channel *channel );
extern int channel_key_share ( struct secure_channel *channel,
			       struct exchange_algorithm *exchange,
			       void *public );
extern int channel_key_agree ( struct secure_channel *channel,
			       struct exchange_algorithm *exchange,
			       const void *partner );
extern int channel_bind_verify ( struct secure_channel *channel,
				 struct x509_certificate *identity,
				 struct pubkey_algorithm *pubkey,
				 struct digest_algorithm *digest,
				 const void *value,
				 const struct asn1_cursor *signature );
extern int channel_bind_encrypt ( struct secure_channel *channel,
				  struct x509_certificate *identity,
				  struct exchange_algorithm *exchange,
				  struct pubkey_algorithm *pubkey,
				  struct asn1_builder *ciphertext );
extern int channel_save ( struct secure_channel *channel,
			  struct secure_preshared_identity *psid );
extern int channel_load ( struct secure_channel *channel,
			  struct secure_preshared_identity *psid );
extern int channel_confirm ( struct secure_channel *channel,
			     const void *auth, size_t len );
extern int channel_establish ( struct secure_channel *channel,
			       const char *name, struct x509_root *root );
extern int channel_set_cipher ( struct secure_channel *channel,
				struct secure_pipe *pipe,
				struct cipher_algorithm *cipher,
				const void *key, size_t len );
extern int channel_open ( struct secure_channel *channel );
extern void channel_reopen ( struct secure_channel *channel );
extern void channel_close ( struct secure_channel *channel );

#endif /* _IPXE_CHANNEL_H */
