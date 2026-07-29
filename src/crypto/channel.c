/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <string.h>
#include <errno.h>
#include <ipxe/malloc.h>
#include <ipxe/crypto.h>
#include <ipxe/hkdf.h>
#include <ipxe/rbg.h>
#include <ipxe/channel.h>

/** @file
 *
 * Secure channel abstraction
 *
 * We define a concept of a secure channel as comprising a pair of
 * ciphers (one for transmit, one for receive) together with the
 * cryptographic state required to establish that these ciphers may be
 * used for encrypted communication with a trusted peer.  The model is
 * loosely constructed as a generalisation of TLS minus the protocol
 * specifics.
 *
 * The channel starts out with both ciphers operating in plaintext
 * mode.  Over this initially unencrypted channel, a shared secret is
 * agreed, bound to a peer identity, and used to derive keys for the
 * transmit and receive ciphers.
 *
 * The major channel operations are:
 *
 *   - Key exchange: a shared secret is agreed (e.g. via Ephemeral
 *     Diffie-Hellman key exchange)
 *
 *   - Key binding: the shared secret is bound to a peer identity
 *     (i.e. to a long-term public key as recorded in an X.509
 *     certificate)
 *
 *   - Key derivation: the transmit and receive cipher keys are
 *     derived from the shared secret
 *
 *   - Key confirmation: the peer demonstrates possession of the
 *     identity-bound shared secret
 *
 *   - Identity validation: the long-term public key representing the
 *     peer identity is validated as being trusted and as matching the
 *     required identity (e.g. a TLS host name)
 *
 * The channel maintains the following security properties:
 *
 *   - "keyed": a boolean flag indicating that the channel contains
 *     key material derived from the shared secret
 *
 *   - "bound": a certificate reference representing the peer identity
 *     to which the shared secret has been bound
 *
 *   - "confirmed": a certificate reference representing the peer
 *     identity that has demonstrated possession of the identity-bound
 *     shared secret
 *
 *   - "established": a certificate reference representing the peer
 *     identity that has demonstrated possession of the identity-bound
 *     shared secret and has been verified as valid
 *
 * The peer identities are stored as references to X.509 certificates.
 * The secure channel conceptual model is agnostic about how peer
 * identities may be represented, but X.509 certificates are most
 * convenient for use within iPXE.  Validation of the certificate
 * chain is outside the scope of the secure channel and must be
 * performed separately.
 *
 * When the channel has a "confirmed" certificate reference and that
 * certificate has separately been marked as valid, the secure channel
 * may become established and may subsequently be used to transmit and
 * receive application data.
 *
 * The channel is a passive entity: it does not itself initiate any
 * messages.  All communication is driven by the consumer (such as the
 * TLS protocol).  The set of operations described above that are used
 * to establish a secure channel must be performed by the consumer
 * according to its protocol definition.
 *
 * The order of these operations may vary based upon both the protocol
 * definition and the options negotiated at runtime with the peer.
 * The secure channel abstraction is responsible for maintaining the
 * listed security properties and determining whether or not the
 * required security conditions have been met, regardless of the order
 * in which operations are performed by the consumer.
 *
 */

/* Disambiguate the various error causes */
#define ENOTTY_TRANSPORT __einfo_error ( EINFO_ENOTTY_TRANSPORT )
#define EINFO_ENOTTY_TRANSPORT \
	__einfo_uniqify ( EINFO_ENOTTY, 0x01, "Untransportable key" )
#define EPERM_NOT_CONFIDENTIAL __einfo_error ( EINFO_EPERM_NOT_CONFIDENTIAL )
#define EINFO_EPERM_NOT_CONFIDENTIAL \
	__einfo_uniqify ( EINFO_EPERM, 0x01, "Not confidential" )
#define EPERM_NOT_VALID __einfo_error ( EINFO_EPERM_NOT_VALID )
#define EINFO_EPERM_NOT_VALID \
	__einfo_uniqify ( EINFO_EPERM, 0x02, "Peer certificate not valid" )
#define EPROTO_NOT_KEYED __einfo_error ( EINFO_EPROTO_NOT_KEYED )
#define EINFO_EPROTO_NOT_KEYED \
	__einfo_uniqify ( EINFO_EPROTO, 0x01, "No shared key material" )
#define EPROTO_NOT_BOUND __einfo_error ( EINFO_EPROTO_NOT_BOUND )
#define EINFO_EPROTO_NOT_BOUND \
	__einfo_uniqify ( EINFO_EPROTO, 0x02, "No bound peer identity" )
#define EPROTO_NOT_CONFIRMED __einfo_error ( EINFO_EPROTO_NOT_CONFIRMED )
#define EINFO_EPROTO_NOT_CONFIRMED \
	__einfo_uniqify ( EINFO_EPROTO, 0x03, "No confirmed peer identity" )

static void channel_unbind ( struct secure_channel *channel );
static void channel_unconfirm ( struct secure_channel *channel );
static void channel_unestablish ( struct secure_channel *channel );

/*****************************************************************************
 *
 * Ephemeral secrets
 *
 *****************************************************************************
 *
 * The channel maintains a single ephemeral master secret that can be
 * used to derive an arbitrary number of other ephemeral secrets
 * (e.g. for Diffie-Hellman key exchange).
 *
 * The use of a single ephemeral master secret avoids the need for key
 * exchange mechanisms to retain individual copies of their ephemeral
 * private keys, since those private keys can always be regenerated on
 * demand.
 *
 * The ephemeral secret mechanism may also be used to derive other
 * temporary secrets that may be required by the consumer (e.g. TLS
 * client random data, or block cipher initialisation vectors).
 *
 */

/**
 * Initialise ephemeral master secret
 *
 * @v channel		Secure channel
 * @ret rc		Return status code
 */
static int channel_ephemeral_init ( struct secure_channel *channel ) {
	struct digest_algorithm *digest = &channel_ephemeral_algorithm;
	static const char salt[16] = "channel master";
	int rc;

	/* Generate random bits with no additional input and without
	 * prediction resistance
	 */
	if ( ( rc = rbg_generate ( NULL, 0, 0, channel->ephemeral,
				   sizeof ( channel->ephemeral ) ) ) != 0 ) {
		DBGC ( channel, "CHANNEL %p could not generate random data: "
		       "%s\n", channel, strerror ( rc ) );
		return rc;
	}

	/* Generate ephemeral master secret */
	hkdf_extract ( digest, salt, sizeof ( salt ), channel->ephemeral,
		       sizeof ( channel->ephemeral ), channel->ephemeral );

	return 0;
}

/**
 * Generate ephemeral secret
 *
 * @v channel		Secure channel
 * @v info		Additional information
 * @v info_len		Length of additional information
 * @v out		Ephemeral secret to fill in
 * @v len		Length of ephemeral secret
 */
void channel_ephemeral ( struct secure_channel *channel, const void *info,
			 size_t info_len, void *out, size_t len ) {
	struct digest_algorithm *digest = &channel_ephemeral_algorithm;

	/* Additional info should always be provided */
	assert ( info != NULL );
	assert ( info_len > 0 );

	/* Generate from ephemeral master secret and additional information */
	hkdf_expand ( digest, channel->ephemeral, info, info_len, out, len );
}

/**
 * Generate labelled ephemeral secret
 *
 * @v channel		Secure channel
 * @v label		Additional information string
 * @v out		Ephemeral secret to fill in
 * @v len		Length of ephemeral secret
 */
void channel_ephemeral_label ( struct secure_channel *channel,
			       const char *label, void *out, size_t len ) {

	/* Generate from ephemeral master secret and label */
	channel_ephemeral ( channel, label, strlen ( label ), out, len );
	DBGC2 ( channel, "CHANNEL %p ephemeral \"%s\":\n", channel, label );
	DBGC2_HDA ( channel, 0, out, len );
}

/**
 * Replace ephemeral master secret
 *
 * @v channel		Secure channel
 */
static void channel_ephemeral_replace ( struct secure_channel *channel ) {

	/* Generate a new ephemeral master key */
	channel_ephemeral_label ( channel, "channel replace",
				  channel->ephemeral,
				  sizeof ( channel->ephemeral ) );
}

/*****************************************************************************
 *
 * Key exchange
 *
 *****************************************************************************
 *
 * Key exchange (or key transport) is used over the as-yet unencrypted
 * channel to obtain a shared secret (e.g. a TLS pre-master secret
 * obtained using ECDHE).  The shared secret is then used to derive
 * the channel key material (e.g. a TLS master secret).
 *
 * The channel is responsible for generating the ephemeral private
 * keys (and the corresponding public keys), and for handling the
 * underlying mechanics of the key exchange algorithm to obtain a
 * shared secret.
 *
 * The consumer is responsible for incorporating the resulting shared
 * secret into its key schedule to produce the key material required
 * by the consumer to operate the channel (e.g. to derive traffic
 * keys).
 *
 * The channel does not itself dictate how the key material is
 * produced, since the key schedule is necessarily defined by the
 * protocol in use (e.g. TLS).
 *
 * Key exchange alone provides no authentication of the peer.  Key
 * binding (see below) must be used to bind the shared secret to a
 * peer identity.
 *
 */

/**
 * Clear shared secret
 *
 * @v channel		Secure channel
 */
void channel_unkey ( struct secure_channel *channel ) {

	/* Key binding is defined only for the current shared secret,
	 * and so any existing bound peer identity must be cleared.
	 */
	channel_unbind ( channel );

	/* Clear shared key */
	channel->props.keyed = 0;

	/* Reset the key schedule */
	channel->op->reset ( channel );
}

/**
 * Generate private key
 *
 * @v channel		Secure channel
 * @v exchange		Key exchange algorithm
 * @v private		Private key to fill in
 */
static void channel_key_private ( struct secure_channel *channel,
				  struct exchange_algorithm *exchange,
				  void *private ) {
	size_t privsize = exchange->privsize;
	const char *label;

	/* Use exchange algorithm name as additional information */
	label = exchange->name;

	/* (Re)generate private key
	 *
	 * Note that key exchange private keys are always small enough
	 * to fit on the stack, even for algorithms such as FFDHE that
	 * have large public and shared keys.
	 */
	channel_ephemeral_label ( channel, label, private, privsize );
}

/**
 * Share public key
 *
 * @v channel		Secure channel
 * @v exchange		Key exchange algorithm
 * @v public		Public key to fill in
 * @ret rc		Return status code
 */
int channel_key_share ( struct secure_channel *channel,
			struct exchange_algorithm *exchange, void *public ) {
	size_t privsize = exchange->privsize;
	size_t pubsize = exchange->pubsize;
	uint8_t private[privsize];
	int rc;

	/* (Re)generate private key */
	channel_key_private ( channel, exchange, private );

	/* Generate public key */
	if ( ( rc = exchange_share ( exchange, private, public ) ) != 0 ) {
		DBGC ( channel, "CHANNEL %p could not share \"%s\": %s\n",
		       channel, exchange->name, strerror ( rc ) );
		goto err_share;
	}

	/* Show public key (for debugging) */
	DBGC2 ( channel, "CHANNEL %p sharing \"%s\":\n",
		channel, exchange->name );
	DBGC2_HDA ( channel, 0, public, pubsize );

 err_share:
	/* Erase private key */
	memset ( private, 0, sizeof ( private ) );
	return rc;
}

/**
 * Generate and apply shared secret
 *
 * @v channel		Secure channel
 * @v exchange		Key exchange algorithm
 * @v partner		Partner public key
 * @v shared		Shared secret to fill in
 * @ret rc		Return status code
 */
static int channel_key ( struct secure_channel *channel,
			 struct exchange_algorithm *exchange,
			 const void *partner, void *shared ) {
	size_t sharedsize = exchange->sharedsize;
	size_t privsize = exchange->privsize;
	uint8_t private[privsize];
	int accumulated;
	int rc;

	/* (Re)generate private key */
	channel_key_private ( channel, exchange, private );

	/* Generate shared secret */
	if ( ( rc = exchange_agree ( exchange, private, partner,
				     shared ) ) != 0 ) {
		DBGC ( channel, "CHANNEL %p could not agree \"%s\": %s\n",
		       channel, exchange->name, strerror ( rc ) );
		goto err_agree;
	}

	/* Show shared secret (for debugging) */
	DBGC2 ( channel, "CHANNEL %p agreed \"%s\":\n",
		channel, exchange->name );
	DBGC2_HDA ( channel, 0, shared, sharedsize );

	/* Apply shared secret to key schedule */
	accumulated = 0;
	if ( ( rc = channel->op->apply ( channel, exchange, shared,
					 &accumulated ) ) != 0 ) {
		DBGC ( channel, "CHANNEL %p could not apply \"%s\": %s\n",
		       channel, exchange->name, strerror ( rc ) );
		goto err_apply;
	}

	/* If the accumulation flag was not set, then the ability of
	 * the peer to subsequently demonstrate possession of the key
	 * material suffices to demonstrate possession of this shared
	 * secret, but does not necessarily suffice to demonstrate
	 * possession of any previously applied shared secrets.  Any
	 * binding of the peer identity is therefore no longer valid.
	 */
	if ( ! accumulated )
		channel_unbind ( channel );

	/* Channel now possesses a shared secret */
	channel->props.keyed = 1;
	DBGC ( channel, "CHANNEL %p key material obtained via \"%s\"\n",
	       channel, exchange->name );

 err_apply:
 err_agree:
	/* Clear generated shared secret on any error */
	if ( rc != 0 )
		channel_unkey ( channel );
	/* Erase private key */
	memset ( private, 0, sizeof ( private ) );
	return rc;
}

/**
 * Agree shared secret
 *
 * @v channel		Secure channel
 * @v exchange		Key exchange algorithm
 * @v partner		Partner public key
 * @ret rc		Return status code
 */
int channel_key_agree ( struct secure_channel *channel,
			struct exchange_algorithm *exchange,
			const void *partner ) {
	size_t sharedsize = exchange->sharedsize;
	size_t pubsize = exchange->pubsize;
	void *tmp;
	int rc;

	/* Show partner public key (for debugging) */
	DBGC2 ( channel, "CHANNEL %p partner \"%s\":\n",
		channel, exchange->name );
	DBGC2_HDA ( channel, 0, partner, pubsize );

	/* Allocate space for shared secret (may be too large for stack) */
	tmp = zalloc ( sharedsize );
	if ( ! tmp ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Generate and apply shared secret */
	if ( ( rc = channel_key ( channel, exchange, partner, tmp ) ) != 0 )
		goto err_key;

 err_key:
	/* Erase shared secret */
	zfree ( tmp );
 err_alloc:
	return rc;
}

/**
 * Transport shared secret
 *
 * @v channel		Secure channel
 * @v exchange		Key exchange algorithm
 * @v shared		Shared secret to fill in
 * @ret rc		Return status code
 *
 * Key transport is modelled as a key exchange algorithm with a public
 * key size of zero: the shared secret is unilaterally generated by
 * the local endpoint and must then be transported to the peer.
 */
static int channel_key_transport ( struct secure_channel *channel,
				   struct exchange_algorithm *exchange,
				   void *shared ) {
	int rc;

	/* Sanity check */
	if ( ! is_key_transport ( exchange ) ) {
		DBGC ( channel, "CHANNEL %p cannot transport \"%s\"\n",
		       channel, exchange->name );
		return -ENOTTY_TRANSPORT;
	}

	/* Generate and apply shared secret */
	if ( ( rc = channel_key ( channel, exchange, NULL, shared ) ) != 0 )
		return rc;

	return 0;
}

/*****************************************************************************
 *
 * Key binding
 *
 *****************************************************************************
 *
 * The shared secret that results from key exchange must be bound to a
 * peer identity.  This binding represents a delegation of authority
 * from the peer's long-term authentication key (e.g. the RSA private
 * key corresponding to its X.509 certificate) to the shared secret
 * for the purpose of authenticating the channel by subsequently
 * confirming possession of the shared secret.
 *
 * There are two distinct mechanisms through which this delegation may
 * take place:
 *
 *   - If the shared secret is obtained using key exchange, then the
 *     peer may delegate authority to the shared secret by using its
 *     long-term authentication key to sign a digest that covers at
 *     least its own ephemeral public key (e.g. the Diffie-Hellman
 *     parameters provided in a ServerKeyExchange record) and a
 *     challenge nonce (e.g. the TLS client random bytes).  The
 *     signature over its public ephemeral key represents a choice by
 *     the peer to allow possession of the corresponding ephemeral
 *     private key to be used to demonstrate authenticity of the
 *     channel.  The local endpoint trusts the peer's long-term
 *     identity to make this class of choice, and verifies the
 *     signature to authenticate that the peer's long-term identity
 *     made this choice.  The possession of the ephemeral private key
 *     is subsequently demonstrated by key confirmation, which
 *     demonstrates possession of the shared secret (and hence of the
 *     ephemeral private key used to obtain the shared secret).
 *
 *   - If the shared secret is obtained using key transport, then the
 *     local endpoint may delegate authority to the shared secret by
 *     encrypting the unilaterally chosen shared secret using the
 *     peer's long-term authentication key.  The encryption represents
 *     a choice by the local endpoint to allow possession of the
 *     shared secret (as subsequently demonstrated by key
 *     confirmation) to be used as evidence of possession of the
 *     peer's long-term authentication key.
 *
 * In both cases, the end result is the same: the shared secret
 * (whether exchanged or transported) may be relied upon to
 * authenticate the channel as being connected to the peer.
 *
 * The shared secret may be bound to an identity that has not yet been
 * validated.  The consumer is responsible for separately performing
 * validation of the identity (e.g. validation of the full X.509
 * certificate chain, including OCSP checks) at some point before
 * relying upon the channel to protect application data.
 *
 */

/**
 * Clear bound peer identity
 *
 * @v channel		Secure channel
 */
static void channel_unbind ( struct secure_channel *channel ) {

	/* Key confirmation is defined only for the currently bound
	 * identity, and so any existing confirmed peer identity must
	 * be cleared.
	 */
	channel_unconfirm ( channel );

	/* Drop and clear reference */
	x509_put ( channel->props.bound );
	channel->props.bound = NULL;
}

/**
 * Set bound peer identity
 *
 * @v channel		Secure channel
 * @v identity		Identity to be bound
 * @ret rc		Return status code
 */
static int channel_bind ( struct secure_channel *channel,
			  struct x509_certificate *identity ) {

	/* Clear any existing binding */
	channel_unbind ( channel );

	/* Fail if there is no shared secret to be bound */
	if ( ! channel->props.keyed ) {
		DBGC ( channel, "CHANNEL %p cannot bind to empty key\n",
		       channel );
		return -EPROTO_NOT_KEYED;
	}

	/* Record bound peer identity */
	channel->props.bound = x509_get ( identity );
	DBGC ( channel, "CHANNEL %p key material bound to \"%s\"\n",
	       channel, x509_name ( identity ) );

	return 0;
}

/**
 * Bind peer identity via ephemeral public key signature verification
 *
 * @v channel		Secure channel
 * @v identity		Identity to be bound
 * @v pubkey		Public-key algorithm
 * @v digest		Digest algorithm
 * @v value		Digest value (must cover peer's ephemeral public key)
 * @v signature		Signature
 * @ret rc		Return status code
 *
 * The peer may delegate authority to the shared secret by signing a
 * digest that covers at least its own ephemeral public key (e.g. the
 * Diffie-Hellman parameters provided in a ServerKeyExchange record)
 * and a challenge nonce (e.g. the TLS client random bytes).
 *
 * The consumer asserts to the secure channel that the digest covers
 * at least the peer's ephemeral public key and a nonce.  The digest
 * may cover arbitrary additional information (e.g. the entire
 * transcript hash as signed by a CertificateVerify record).
 *
 * This assertion is an unverifiable promise made by the consumer:
 * there is no way for the secure channel itself to determine what is
 * covered by the digest.
 */
int channel_bind_verify ( struct secure_channel *channel,
			  struct x509_certificate *identity,
			  struct pubkey_algorithm *pubkey,
			  struct digest_algorithm *digest, const void *value,
			  const struct asn1_cursor *signature ) {
	const struct asn1_cursor *key = &identity->subject.public_key.raw;
	int rc;

	/* Verify signature */
	if ( ( rc = pubkey_verify ( pubkey, key, digest, value,
				    signature ) ) != 0 ) {
		DBGC ( channel, "CHANNEL %p could not verify binding: %s\n",
		       channel, strerror ( rc ) );
		return rc;
	}

	/* Bind to the identity that signed the shared secret */
	if ( ( rc = channel_bind ( channel, identity ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Bind peer identity via shared secret encryption
 *
 * @v channel		Secure channel
 * @v identity		Identity to be bound
 * @v exchange		Key exchange algorithm
 * @v pubkey		Public-key algorithm
 * @v ciphertext	Encrypted shared secret
 * @ret rc		Return status code
 *
 * The local endpoint may delegate authority to the shared secret by
 * encrypting a unilaterally chosen shared secret (e.g. a classic TLS
 * static RSA pre-master secret).
 */
int channel_bind_encrypt ( struct secure_channel *channel,
			   struct x509_certificate *identity,
			   struct exchange_algorithm *exchange,
			   struct pubkey_algorithm *pubkey,
			   struct asn1_builder *ciphertext ) {
	const struct asn1_cursor *key = &identity->subject.public_key.raw;
	size_t sharedsize = exchange->sharedsize;
	struct asn1_cursor plaintext;
	void *tmp;
	int rc;

	/* Allocate space for shared secret (may be too large for stack) */
	tmp = zalloc ( sharedsize );
	if ( ! tmp ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	plaintext.data = tmp;
	plaintext.len = sharedsize;

	/* Generate unencrypted shared secret */
	if ( ( rc = channel_key_transport ( channel, exchange, tmp ) ) != 0 )
		goto err_transport;

	/* Encrypt shared secret */
	if ( ( rc = pubkey_encrypt ( pubkey, key, &plaintext,
				     ciphertext ) ) != 0 ) {
		DBGC ( channel, "CHANNEL %p could not encrypt \"%s\": %s\n",
		       channel, exchange->name, strerror ( rc ) );
		goto err_encrypt;
	}

	/* Bind to the identity that can decrypt the shared secret */
	if ( ( rc = channel_bind ( channel, identity ) ) != 0 )
		goto err_bind;

 err_bind:
 err_encrypt:
 err_transport:
	/* Clear the generated shared secret on any error */
	if ( rc != 0 )
		channel_unkey ( channel );
	/* Erase unencrypted shared secret */
	zfree ( tmp );
 err_alloc:
	return rc;
}

/*****************************************************************************
 *
 * Pre-shared keys
 *
 *****************************************************************************
 *
 * Once a shared secret has been bound to a peer identity, it may be
 * saved as a pre-shared key alongside the bound peer identity.  A
 * future secure channel may resume the key schedule from this
 * pre-shared and already bound key.
 *
 * The consumer is responsible for ensuring that the ability to
 * demonstrate possession of the key material after resumption implies
 * possession of the key material that was previously saved (and so
 * implies possession of the original shared secret).
 *
 * The consumer may choose to save a pre-shared key at any point after
 * the peer identity has been bound.  A successful resumption will
 * restore the previously bound peer identity but will clear the
 * confirmed and established peer identities.
 *
 * Key confirmation to demonstrate possession of the key material is
 * still required after resuming a secure channel from a pre-shared
 * key.
 *
 * The establishment check is also still required after resuming a
 * secure channel from a pre-shared key.  It is technically possible
 * (though unlikely in practice) for the establishment check to use a
 * new required peer identity name or root of trust.
 *
 * Support for pre-shared keys is optional.  The consumer may choose
 * not to support the use of pre-shared keys.
 *
 */

/**
 * Save a pre-shared key
 *
 * @v channel		Secure channel
 * @v psid		Pre-shared bound peer identity
 * @ret rc		Return status code
 */
int channel_save ( struct secure_channel *channel,
		   struct secure_preshared_identity *psid ) {
	struct x509_certificate *identity;
	int rc;

	/* Clear any existing pre-shared bound peer identity */
	channel_clear_preshared ( psid );

	/* Fail if there is no bound peer identity to save */
	identity = channel->props.bound;
	if ( ! identity ) {
		DBGC ( channel, "CHANNEL %p cannot create pre-shared key "
		       "for unbound peer\n", channel );
		return -EPROTO_NOT_BOUND;
	}

	/* Fail if consumer does not support pre-shared keys */
	if ( ! channel->op->save ) {
		DBGC ( channel, "CHANNEL %p does not support pre-shared "
		       "keys\n", channel );
		return -ENOTSUP;
	}

	/* Save pre-shared key */
	if ( ( rc = channel->op->save ( channel, psid ) ) != 0 ) {
		DBGC ( channel, "CHANNEL %p could not save pre-shared "
		       "key: %s\n", channel, strerror ( rc ) );
		return rc;
	}

	/* Record pre-shared bound peer identity */
	psid->bound = x509_get ( identity );
	DBGC ( channel, "CHANNEL %p key material saved for \"%s\"\n",
	       channel, x509_name ( identity ) );

	return 0;
}

/**
 * Load a pre-shared key
 *
 * @v channel		Secure channel
 * @v psid		Pre-shared bound peer identity
 * @ret rc		Return status code
 */
int channel_load ( struct secure_channel *channel,
		   struct secure_preshared_identity *psid ) {
	struct x509_certificate *identity;
	int rc;

	/* Clear any existing shared secret */
	channel_unkey ( channel );

	/* Fail if there is no bound peer identity to resume */
	identity = psid->bound;
	if ( ! identity ) {
		DBGC ( channel, "CHANNEL %p cannot resume from empty "
		       "pre-shared key\n", channel );
		rc = -EPROTO_NOT_BOUND;
		goto err_empty;
	}

	/* Fail if consumer does not support pre-shared keys */
	if ( ! channel->op->load ) {
		DBGC ( channel, "CHANNEL %p does not support pre-shared "
		       "keys\n", channel );
		rc = -ENOTSUP;
		goto err_unsupported;
	}

	/* Load pre-shared key */
	if ( ( rc = channel->op->load ( channel, psid ) ) != 0 ) {
		DBGC ( channel, "CHANNEL %p could not load pre-shared key: "
		       "%s\n", channel, strerror ( rc ) );
		goto err_load;
	}

	/* Channel now possesses a shared secret */
	channel->props.keyed = 1;
	DBGC ( channel, "CHANNEL %p key material obtained from pre-shared "
	       "key\n", channel );

	/* Bind to the identity that was bound to the pre-shared key */
	if ( ( rc = channel_bind ( channel, identity ) ) != 0 )
		goto err_bind;

	return 0;

 err_bind:
 err_load:
 err_unsupported:
 err_empty:
	channel_unkey ( channel );
	return rc;
}

/*****************************************************************************
 *
 * Key confirmation
 *
 *****************************************************************************
 *
 * The peer must authenticate the channel by demonstrating its
 * possession of the current key material that was derived from the
 * identity-bound shared secret in accordance with the key schedule.
 *
 * The peer may demonstrate its possession of the key material by
 * constructing and sending an authenticator value that can be
 * successfully verified by the local endpoint using the same key
 * material.
 *
 * The receive pipe must already have transitioned to a cipher capable
 * of providing confidentiality before the authenticator value is
 * received, so that the authentication of the channel meaningfully
 * endures beyond the receipt of the authenticator value itself.
 *
 * The key may be confirmed for an identity that has not yet been
 * validated.  The consumer is responsible for separately performing
 * validation of the identity (e.g. validation of the full X.509
 * certificate chain, including OCSP checks) at some point before
 * relying upon the channel to protect application data.
 *
 */

/**
 * Clear confirmed peer identity
 *
 * @v channel		Secure channel
 */
static void channel_unconfirm ( struct secure_channel *channel ) {

	/* Establishment is defined only for the currently confirmed
	 * identity, and so any existing established identity must be
	 * cleared.
	 */
	channel_unestablish ( channel );

	/* Drop and clear reference */
	x509_put ( channel->props.confirmed );
	channel->props.confirmed = NULL;
}

/**
 * Confirm peer identity
 *
 * @v channel		Secure channel
 * @v auth		Authenticator value
 * @v len		Length of authenticator value
 * @ret rc		Return status code
 *
 * The consumer asserts to the secure channel that the authenticator
 * value was received after the receive pipe was transitioned to a
 * cipher capable of providing confidentiality.
 *
 * This assertion is an unverifiable promise made by the consumer:
 * there is no way for the secure channel itself to determine when the
 * authenticator value was received.
 */
int channel_confirm ( struct secure_channel *channel,
		      const void *auth, size_t len ) {
	struct x509_certificate *identity;
	int rc;

	/* Clear any existing confirmed peer identity */
	channel_unconfirm ( channel );

	/* Fail if there is no bound peer identity to authenticate */
	identity = channel->props.bound;
	if ( ! identity ) {
		DBGC ( channel, "CHANNEL %p cannot authenticate unbound "
		       "peer\n", channel );
		return -EPROTO_NOT_BOUND;
	}

	/* Sanity check: binding is not possible without keying */
	assert ( channel->props.keyed );

	/* Require key confirmation over a confidential channel */
	if ( ! channel->rx.cipher->confidential ) {
		DBGC ( channel, "CHANNEL %p cannot confirm key with a "
		       "non-confidential cipher\n", channel );
		return -EPERM_NOT_CONFIDENTIAL;
	}

	/* Verify authentication data */
	if ( ( rc = channel->op->verify ( channel, auth, len ) ) != 0 ) {
		DBGC ( channel, "CHANNEL %p failed to authenticate: %s\n",
		       channel, strerror ( rc ) );
		return rc;
	}

	/* Record bound peer identity as confirmed peer identity */
	channel->props.confirmed = x509_get ( identity );
	DBGC ( channel, "CHANNEL %p key material confirmed for \"%s\"\n",
	       channel, x509_name ( identity ) );

	return 0;
}

/*****************************************************************************
 *
 * Establishment
 *
 *****************************************************************************
 *
 * The channel may be used for application data once the peer identity
 * has been validated, the peer has authenticated the channel via key
 * confirmation, and both pipes have been transitioned to ciphers
 * capable of providing confidentiality.
 *
 * Validating the peer identity (i.e. validating its X.509 certificate
 * chain) is outside the scope of the secure channel and must be
 * performed separately by the consumer.  Validation is an
 * asynchronous operation that will typically require network
 * communication (e.g. to perform OCSP checks).
 *
 * Once the consumer knows that validation has completed and that the
 * channel has been authenticated, it may mark the channel as
 * established.
 *
 * Establishment performs a one-off check that the confirmed peer
 * identity exists, matches the name required by the consumer, and is
 * valid at this point in time.  Future changes to the validity of the
 * peer identity (e.g. due to certificate expiry) will not cause the
 * channel to cease to be established.
 *
 */

/**
 * Clear established peer identity
 *
 * @v channel		Secure channel
 */
static void channel_unestablish ( struct secure_channel *channel ) {

	/* Drop and clear reference */
	x509_put ( channel->props.established );
	channel->props.established = NULL;
}

/**
 * Establish channel as trusted for application data
 *
 * @v channel		Secure channel
 * @v name		Required peer identity name
 * @v root		Root certificate list, or NULL to use default
 * @ret rc		Return status code
 */
int channel_establish ( struct secure_channel *channel, const char *name,
			struct x509_root *root ) {
	struct x509_certificate *identity;
	int rc;

	/* Clear any existing established peer identity */
	channel_unestablish ( channel );

	/* Fail if there is no confirmed peer identity to establish */
	identity = channel->props.confirmed;
	if ( ! identity ) {
		DBGC ( channel, "CHANNEL %p cannot establish unconfirmed "
		       "peer\n", channel );
		return -EPROTO_NOT_CONFIRMED;
	}

	/* Sanity check: confirmation is not possible without binding */
	assert ( channel->props.bound == identity );
	assert ( channel->props.keyed );

	/* Fail if ciphers are still operating in plaintext mode */
	if ( ! ( channel->tx.cipher->confidential &&
		 channel->rx.cipher->confidential ) ) {
		DBGC ( channel, "CHANNEL %p cannot establish channel with a "
		       "non-confidential cipher\n", channel );
		return -EPERM_NOT_CONFIDENTIAL;
	}

	/* Fail if confirmed peer identity name is incorrect */
	if ( ( rc = x509_check_name ( identity, name ) ) != 0 ) {
		DBGC ( channel, "CHANNEL %p identity \"%s\" does not match "
		       "\"%s\": %s\n", channel, x509_name ( identity ),
		       name, strerror ( rc ) );
		return rc;
	}

	/* Fail if confirmed peer identity has not been validated */
	if ( ! x509_is_valid ( identity, root ) ) {
		DBGC ( channel, "CHANNEL %p identity \"%s\" has not been "
		       "validated\n", channel, x509_name ( identity ) );
		return -EPERM_NOT_VALID;
	}

	/* Record confirmed peer identity as established peer identity */
	channel->props.established = x509_get ( identity );
	DBGC ( channel, "CHANNEL %p established for \"%s\"\n",
	       channel, x509_name ( identity ) );

	return 0;
}

/*****************************************************************************
 *
 * Cipher operation
 *
 *****************************************************************************
 *
 */

/**
 * Encrypt or decrypt data via dead cipher
 *
 * @v cipher		Cipher algorithm
 * @v ctx		Context
 * @v src		Data to encrypt or decrypt
 * @v dst		Buffer for encrypted or decrypted data
 * @v len		Length of data
 */
static void channel_dead_crypt ( struct cipher_algorithm *cipher __unused,
				 void *ctx __unused, const void *src __unused,
				 void *dst, size_t len ) {
	static const uint8_t pattern[] = { 0xde, 0xad, 0xc1, 0xfa };
	uint8_t *bytes = dst;
	unsigned int i;

	/* Fill destination with fixed and easily spotted pattern */
	for ( i = 0 ; i < len ; i++ )
		bytes[i] = pattern[ i % sizeof ( pattern ) ];
}

/**
 * Dead cipher
 *
 * We activate this cipher (rather than the null cipher) upon any
 * cipher-related failure, to guard against code paths that may fail
 * to check for cipher errors.
 */
static struct cipher_algorithm channel_dead_cipher = {
	.name = "dead",
	.ctxsize = 0,
	.blocksize = 1,
	.alignsize = 1,
	.authsize = 0,
	.confidential = 0,
	.setkey = cipher_null_setkey,
	.setiv = cipher_null_setiv,
	.encrypt = channel_dead_crypt,
	.decrypt = channel_dead_crypt,
	.auth = cipher_null_auth,
};

/**
 * Get pipe name (for debugging)
 *
 * @v channel		Secure channel
 * @v pipe		Secure channel pipe
 * @ret name		Pipe name
 */
static inline const char * channel_pipe_name ( struct secure_channel *channel,
					       struct secure_pipe *pipe ) {

	return ( ( pipe == &channel->tx ) ? "TX" :
		 ( pipe == &channel->rx ) ? "RX" : "INVALID" );
}

/**
 * Clear cipher algorithm
 *
 * @v pipe		Secure channel pipe
 */
static void channel_clear_cipher ( struct secure_pipe *pipe ) {

	/* Zero and free any existing cipher context */
	zfree ( pipe->ctx );
	pipe->ctx = NULL;

	/* Default to the dead cipher until set */
	pipe->cipher = &channel_dead_cipher;
}

/**
 * Set cipher algorithm and key
 *
 * @v channel		Secure channel
 * @v pipe		Secure channel pipe
 * @v cipher		Cipher algorithm
 * @v key		Key
 * @v len		Length of key
 * @ret rc		Return status code
 *
 * The consumer asserts to the secure channel that the key is
 * ultimately derived from the shared secret that was previously
 * provided to the consumer by the secure channel, i.e. that
 * possession of the cipher key implies possession of the shared
 * secret.
 *
 * This assertion is an unverifiable promise made by the consumer: we
 * can check that a shared secret exists, but there is no way for the
 * secure channel itself to determine how the cipher key is derived.
 */
int channel_set_cipher ( struct secure_channel *channel,
			 struct secure_pipe *pipe,
			 struct cipher_algorithm *cipher,
			 const void *key, size_t len ) {
	int rc;

	/* Zero and free any existing cipher context */
	channel_clear_cipher ( pipe );

	/* Fail if there is no shared secret from which a key could derive */
	if ( ! channel->props.keyed ) {
		DBGC ( channel, "CHANNEL %p refusing to use definitely "
		       "non-derived shared secret\n", channel );
		rc = -EPROTO_NOT_KEYED;
		goto err_keyed;
	}

	/* Refuse to explicitly set a non-confidential cipher */
	if ( ! cipher->confidential ) {
		DBGC ( channel, "CHANNEL %p %s refusing to use "
		       "non-confidential cipher \"%s\"\n", channel,
		       channel_pipe_name ( channel, pipe ), cipher->name );
		rc = -EPERM_NOT_CONFIDENTIAL;
		goto err_confidential;
	}
	pipe->cipher = cipher;

	/* Allocate new context */
	pipe->ctx = zalloc ( cipher->ctxsize );
	if ( ! pipe->ctx ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Set cipher key */
	if ( ( rc = cipher_setkey ( cipher, pipe->ctx, key, len ) ) != 0 ) {
		DBGC ( channel, "CHANNEL %p %s could not set \"%s\" key: %s\n",
		       channel, channel_pipe_name ( channel, pipe ),
		       cipher->name, strerror ( rc ) );
		goto err_setkey;
	}

	DBGC ( channel, "CHANNEL %p %s using \"%s\" with %zd-bit key\n",
	       channel, channel_pipe_name ( channel, pipe ),
	       cipher->name, ( 8 * len ) );
	return 0;

 err_setkey:
 err_alloc:
 err_confidential:
 err_keyed:
	channel_clear_cipher ( pipe );
	return rc;
}

/*****************************************************************************
 *
 * Life cycle management
 *
 *****************************************************************************
 *
 */

/**
 * Reset secure channel
 *
 * @v channel		Secure channel
 */
static void channel_reset ( struct secure_channel *channel ) {

	/* Reset ciphers to plaintext */
	channel_clear_cipher ( &channel->tx );
	channel_clear_cipher ( &channel->rx );
	channel->tx.cipher = &cipher_null;
	channel->rx.cipher = &cipher_null;
	assert ( channel->tx.ctx == NULL );
	assert ( channel->rx.ctx == NULL );

	/* Clear security properties */
	channel_unkey ( channel );
	assert ( channel->props.keyed == 0 );
	assert ( channel->props.bound == NULL );
	assert ( channel->props.confirmed == NULL );
	assert ( channel->props.established == NULL );
}

/**
 * Open secure channel
 *
 * @v channel		Secure channel
 * @ret rc		Return status code
 */
int channel_open ( struct secure_channel *channel ) {
	int rc;

	/* Sanity checks */
	assert ( channel != NULL );
	assert ( channel->op != NULL );
	assert ( channel->op->reset != NULL );
	assert ( channel->op->apply != NULL );
	assert ( channel->op->verify != NULL );
	assert ( channel->tx.ctx == NULL );
	assert ( channel->rx.ctx == NULL );

	/* Reset ciphers and security properties */
	channel_reset ( channel );

	/* Initialise ephemeral master secret */
	if ( ( rc = channel_ephemeral_init ( channel ) ) != 0 )
		return rc;

	DBGC ( channel, "CHANNEL %p opened\n", channel );
	return 0;
}

/**
 * Reopen secure channel
 *
 * @v channel		Secure channel
 *
 * The channel will be returned to a freshly opened state (including a
 * new ephemeral master secret), but with any existing cipher state
 * retained.  All security properties will be cleared.
 *
 * This allows a new secure channel to be established with either or
 * both of the transmit and receive pipes having already transitioned
 * to a cipher capable of providing confidentiality.
 *
 * The consumer may choose to reopen a fully established channel while
 * leaving the cipher keys intact (as is done in TLS renegotiation),
 * in which case it may choose to treat the channel as remaining
 * established (to the old peer identity) at least until the point
 * that the cipher keys are next changed (and possibly further,
 * depending upon the protocol design).  The secure channel itself
 * does not attempt to model any such choice by the consumer:
 * reopening will clear all security properties including the
 * established peer identity.
 */
void channel_reopen ( struct secure_channel *channel ) {

	/* Clear security properties */
	channel_unkey ( channel );

	/* Replace ephemeral master secret
	 *
	 * We choose to replace rather than reinitialise the ephemeral
	 * master secret so that this function may be used from code
	 * paths that cannot allow for failure (such as when closing
	 * the channel).
	 */
	channel_ephemeral_replace ( channel );

	DBGC ( channel, "CHANNEL %p reopened\n", channel );
}

/**
 * Close secure channel
 *
 * @v channel		Secure channel
 *
 * All secret values held by the secure channel will be destroyed,
 * including the ephemeral master secret.
 *
 * This function may safely be called on a channel that has already
 * been closed.
 */
void channel_close ( struct secure_channel *channel ) {

	/* Reset channel */
	channel_reset ( channel );

	/* Replace ephemeral master secret
	 *
	 * We choose to replace rather than zero the ephemeral master
	 * secret.  For any channel that has been successfully opened,
	 * this ensures that an erroneous reuse of the channel after
	 * it has been closed cannot lead to the use of predictable
	 * ephemeral secrets (derived from an all-zero value), or to
	 * reuse of its previous ephemeral secrets.
	 */
	channel_ephemeral_replace ( channel );

	DBGC ( channel, "CHANNEL %p closed\n", channel );
}
