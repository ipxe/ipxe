/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
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

/** @file
 *
 * TLS key schedules
 *
 * The TLS key schedule is responsible for maintaining the running
 * handshake transcript digest, for deriving key material from the
 * shared secret that was negotiated by a key exchange mechanism, and
 * for performing all cryptographic calculations required by the TLS
 * protocol.
 *
 * A single abstraction of a key schedule is defined that is
 * independent of the TLS protocol version.  Three implementations of
 * this abstraction are provided:
 *
 *   - a TLS version 1.3 key schedule using HKDF
 *
 *   - a TLS version 1.2 key schedule using PRF based on P_Hash()
 *
 *   - a TLS version 1.0/1.1 key schedule using PRF based on P_MD5()+P_SHA1()
 *
 * The basic set of operations is as required by the TLS protocol:
 *
 *   - Start the key schedule by specifying the key schedule
 *     implementation to be used along with the handshake digest
 *     algorithm
 *
 *   - Add each handshake record to the running transcript digest
 *
 *   - Apply a shared secret (e.g. a pre-master secret that was agreed
 *     using Ephemeral Diffie-Hellman key exchange)
 *
 *   - Derive a master secret from the shared secret
 *
 *   - Derive client and server verification data for the Finished
 *     handshake records
 *
 *   - Derive cipher keys, fixed IVs, and MAC secrets
 *
 *   - Derive digest values to be signed or verified (e.g. for
 *     CertificateVerify and ServerKeyExchange)
 *
 *   - Save pre-shared key for future session resumption
 *
 *   - Load pre-shared key to resume a session
 *
 * The key schedule is responsible only for raw key material: it has
 * no notion of the peer identity and so is fundamentally anonymous in
 * nature.
 *
 * The separate secure channel abstraction handles all concepts of
 * peer identity.  The key schedule provides the cryptographic
 * operations and properties that are documented as required by the
 * secure channel, e.g. the calculations required to demonstrate or
 * verify possession of key material.
 *
 */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/malloc.h>
#include <ipxe/hkdf.h>
#include <ipxe/hmac.h>
#include <ipxe/md5_sha1.h>
#include <ipxe/tlskey.h>

/*****************************************************************************
 *
 * Endpoints
 *
 *****************************************************************************
 */

/** Client endpoint */
const struct tls_endpoint tls_client = {
	.index = TLS_CLIENT,
	.name = "client",
};

/** Server endpoint */
const struct tls_endpoint tls_server = {
	.index = TLS_SERVER,
	.name = "server",
};

/** Endpoint list */
static const struct tls_endpoint *tls_endpoint[] = {
	[TLS_CLIENT] = &tls_client,
	[TLS_SERVER] = &tls_server,
};

/*****************************************************************************
 *
 * Traffic phases
 *
 *****************************************************************************
 */

/** A TLS traffic phase */
struct tls_phase {
	/** Name */
	const char *name;
	/** Key expansion label */
	const char label[ 3 /* "[e|hs|ap]" + NUL */ ];
	/** Required key derivation function flags */
	uint8_t flags;
};

/** Early traffic phase */
const struct tls_phase tls_early = {
	.name = "early",
	.label = "e",
	.flags = TLSKEY_KDF_KEYED,
};

/** Handshake traffic phase */
const struct tls_phase tls_handshake = {
	.name = "handshake",
	.label = "hs",
	.flags = TLSKEY_KDF_KEYED,
};

/** Application traffic phase */
const struct tls_phase tls_application = {
	.name = "application",
	.label = "ap",
	.flags = ( TLSKEY_KDF_KEYED | TLSKEY_KDF_MASTER ),
};

/*****************************************************************************
 *
 * Lifecycle management
 *
 *****************************************************************************
 */

/**
 * Start key schedule
 *
 * @v tlskey		Key schedule
 * @v op		Key schedule operations
 * @v digest		Digest algorithm
 * @v nonce		Local endpoint random bytes
 * @ret rc		Return status code
 *
 * The random bytes must be the random bytes that the local endpoint
 * will subsequently incorporate within its own ClientHello (or
 * ServerHello, if acting as a server).  The caller must guarantee
 * that these are fresh and are unpredictable by the peer.  A freshly
 * generated secure channel ephemeral secret can be used to fulfil
 * this requirement.
  */
int tlskey_start ( struct tls_key_schedule *tlskey,
		   const struct tls_key_schedule_operations *op,
		   struct digest_algorithm *digest,
		   const struct tls_random *nonce ) {
	size_t digestsize = digest->digestsize;
	size_t ctxsize = digest->ctxsize;
	size_t secretsize;
	size_t total;
	void *dynamic;
	int rc;

	/* Stop any existing running key schedule */
	tlskey_stop ( tlskey );

	/* Sanity check */
	if ( ! digestsize ) {
		rc = -ENOTTY;
		goto err_sanity;
	}

	/* Determine required sizes */
	secretsize = op->secretsize ( digest );
	if ( ! secretsize ) {
		DBGC ( tlskey, "TLSKEY %p cannot use %s with %s\n",
		       tlskey, op->name, digest->name );
		rc = -ENOTSUP;
		goto err_secretsize;
	}
	total = ( secretsize +	/* secret */
		  ctxsize +	/* transcript.ctx */
		  digestsize +	/* transcript.running */
		  digestsize	/* transcript.finishing */ );

	/* Allocate dynamic storage */
	dynamic = zalloc ( total );
	if ( ! dynamic ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Initialise schedule */
	tlskey->op = op;
	tlskey->digest = digest;
	tlskey->secretsize = secretsize;
	DBGC ( tlskey, "TLSKEY %p using %s with %s\n",
	       tlskey, tlskey->op->name, tlskey->digest->name );

	/* Assign dynamic storage */
	tlskey->dynamic = dynamic;
	tlskey->secret = dynamic;		dynamic += secretsize;
	tlskey->transcript.ctx = dynamic;	dynamic += ctxsize;
	tlskey->transcript.running = dynamic;	dynamic += digestsize;
	tlskey->transcript.finishing = dynamic;	dynamic += digestsize;
	assert ( dynamic == ( tlskey->dynamic + total ) );

	/* Initialise transcript digest */
	digest_init ( digest, tlskey->transcript.ctx );
	tlskey_digest ( tlskey, NULL, 0 );
	memcpy ( &tlskey->nonce, nonce, sizeof ( tlskey->nonce ) );
	DBGC ( tlskey, "TLSKEY %p local nonce:\n", tlskey );
	DBGC_HDA ( tlskey, 0, &tlskey->nonce, sizeof ( tlskey->nonce ) );

	/* Reset key schedule */
	tlskey_reset ( tlskey );

	return 0;

	tlskey_stop ( tlskey );
 err_alloc:
 err_secretsize:
 err_sanity:
	return rc;
}

/**
 * Stop key schedule
 *
 * @v tlskey		Key schedule
 */
void tlskey_stop ( struct tls_key_schedule *tlskey ) {

	/* Clear and free any dynamic storage */
	zfree ( tlskey->dynamic );

	/* Clear key schedule contents */
	memset ( tlskey, 0, sizeof ( *tlskey ) );

	/* Set null digest algorithm */
	tlskey->digest = &digest_null;
}

/*****************************************************************************
 *
 * Handshake transcript digest
 *
 *****************************************************************************
 */

/** A ClientHello or ServerHello handshake record prefix of interest */
struct tlskey_hello {
	uint8_t type;
	uint8_t length[3];
	uint16_t version;
	struct tls_random random;
} __attribute__ (( packed ));

/** Calculate endpoint index from ClientHello or ServerHello record type */
#define TLSKEY_HELLO_IDX( type ) ( (type) - 1 )

/** A Finished handshake record prefix of interest */
struct tlskey_finished {
	uint8_t type;
} __attribute__ (( packed ));

/** Finished record type */
#define TLSKEY_FINISHED 20

/**
 * Process digested ClientHello or ServerHello handshake record
 *
 * @v tlskey		Key schedule
 * @v hello		ClientHello or ServerHello handshake record
 * @v index		Endpoint index
 * @ret flags		Additional transcript flags to set
 */
static unsigned int tlskey_hello ( struct tls_key_schedule *tlskey,
				   const struct tlskey_hello *hello,
				   unsigned int index ) {
	const struct tls_endpoint *end = tls_endpoint[index];
	const struct tls_random *nonce = &tlskey->nonce;
	struct tls_random *random = &tlskey->random[index];
	unsigned int flags = 0;

	/* We check that the local endpoint random bytes have been
	 * incorporated into the running transcript digest, since this
	 * is required in order to provide our non-replayability
	 * guarantees.
	 */
	if ( memcmp ( nonce, &hello->random, sizeof ( *nonce ) ) == 0 )
		flags |= TLSKEY_TSF_NONCED;

	/* We also capture and store the client and server random
	 * bytes from these records, since TLS versions 1.2 and
	 * earlier sometimes require these values separately from the
	 * full running transcript digest.
	 */
	memcpy ( random, &hello->random, sizeof ( *random ) );

	DBGC ( tlskey, "TLSKEY %p %s random bytes%s:\n", tlskey, end->name,
	       ( ( flags & TLSKEY_TSF_NONCED ) ? " (local nonce)" : "" ) );
	DBGC_HDA ( tlskey, 0, random, sizeof ( *random ) );
	return flags;
}

/**
 * Process digested Finished handshake record
 *
 * @v tlskey		Key schedule
 * @ret flags		Additional transcript flags to set
 */
static unsigned int tlskey_finished ( struct tls_key_schedule *tlskey ) {
	struct tls_transcript *transcript = &tlskey->transcript;
	struct digest_algorithm *digest = tlskey->digest;
	size_t digestsize = digest->digestsize;

	/* The TLS version 1.3 key schedule derives application
	 * traffic secrets from the transcript hash up to and
	 * including only the server Finished, but requires the
	 * handshake traffic secret to remain available for
	 * calculating the client Finished.
	 *
	 * If there are any intervening handshake records (e.g. an
	 * EndOfEarlyData) then this makes it impossible to construct
	 * the client application traffic secret without either:
	 *
	 *   - retaining a separate record of the transcript hash up
	 *     to and including the server Finished, or
	 *
	 *   - retaining the client handshake traffic secret after
	 *     calculating the client application traffic secret.
	 *
	 * The TLS version 1.3 key schedule also derives resumption
	 * secrets from NewSessionTicket handshake records that arrive
	 * after the client Finished, but using the transcript hash up
	 * to and including only the client Finished.
	 *
	 * We choose to solve both of these problems simultaneously by
	 * retaining a second "finishing" digest value that mirrors
	 * the continuously running digest value until the first
	 * Finished is digested, and then switches to updating only
	 * when a new Finished is digested.
	 */
	DBGC ( tlskey, "TLSKEY %p finishing digest:\n", tlskey );
	DBGC_HDA ( tlskey, 0, transcript->running, digestsize );
	return TLSKEY_TSF_FINISHED;
}

/**
 * Process digested handshake record
 *
 * @v tlskey		Key schedule
 * @v data		Handshake record
 * @v len		Length of handshake record
 * @ret flags		Additional transcript flags to set
 */
static unsigned int tlskey_handshake ( struct tls_key_schedule *tlskey,
				       const void *data, size_t len ) {
	const struct tlskey_hello *hello;
	const struct tlskey_finished *finished;
	unsigned int index;

	/* Check for ClientHello and ServerHello */
	if ( len >= sizeof ( *hello ) ) {
		hello = data;
		index = TLSKEY_HELLO_IDX ( hello->type );
		if ( ( index == TLS_CLIENT ) || ( index == TLS_SERVER ) )
			return tlskey_hello ( tlskey, hello, index );
	}

	/* Check for Finished */
	if ( len >= sizeof ( *finished ) ) {
		finished = data;
		if ( finished->type == TLSKEY_FINISHED )
			return tlskey_finished ( tlskey );
	}

	return 0;
}

/**
 * Add handshake to running transcript digest
 *
 * @v tlskey		Key schedule
 * @v data		Handshake record
 * @v len		Length of handshake record
 */
void tlskey_digest ( struct tls_key_schedule *tlskey, const void *data,
		     size_t len ) {
	struct tls_transcript *transcript = &tlskey->transcript;
	struct digest_algorithm *digest = tlskey->digest;
	size_t digestsize = digest->digestsize;
	size_t ctxsize = digest->ctxsize;
	unsigned int flags;
	struct {
		uint8_t ctx[ctxsize];
	} tmp;

	/* Append to running transcript digest */
	digest_update ( digest, transcript->ctx, data, len );

	/* Update running transcript digest output */
	memcpy ( tmp.ctx, transcript->ctx, ctxsize );
	digest_final ( digest, tmp.ctx, transcript->running );

	/* Process digested handshake record */
	flags = tlskey_handshake ( tlskey, data, len );
	transcript->flags |= flags;

	/* Update the finishing digest value, if applicable */
	if ( ! ( ( transcript->flags ^ flags ) & TLSKEY_TSF_FINISHED ) ) {
		memcpy ( transcript->finishing, transcript->running,
			 digestsize );
	}

	/* Clear temporary secrets */
	memset ( &tmp, 0, sizeof ( tmp ) );
}

/*****************************************************************************
 *
 * Key material
 *
 *****************************************************************************
 */

/**
 * Expand key material
 *
 * @v tlskey		Key schedule
 * @v secret		Secret
 * @v label		Label string
 * @v seed		Seed material
 * @v seed_len		Length of seed material
 * @v out		Output buffer
 * @v out_len		Length of output buffer
 */
static void tlskey_expand ( struct tls_key_schedule *tlskey,
			    const void *secret, const char *label,
			    const void *seed, size_t seed_len,
			    void *out, size_t out_len ) {
	const struct tls_key_schedule_operations *op = tlskey->op;
	struct digest_algorithm *digest = tlskey->digest;

	/* Sanity check */
	assert ( op != NULL );

	/* Expand key material */
	DBGC2 ( tlskey, "TLSKEY %p expanding seed \"%s\":\n", tlskey, label );
	DBGC2_HDA ( tlskey, 0, seed, seed_len );
	op->expand ( digest, secret, label, seed, seed_len, out, out_len );
	DBGC2 ( tlskey, "TLSKEY %p expanded:\n", tlskey );
	DBGC2_HDA ( tlskey, 0, out, out_len );
}

/**
 * Reset key schedule
 *
 * @v tlskey		Key schedule
 *
 * It is safe to reset a key schedule that has been stopped, or that
 * has not yet been started.
 */
void tlskey_reset ( struct tls_key_schedule *tlskey ) {
	const struct tls_key_schedule_operations *op = tlskey->op;

	/* Do nothing if key schedule is stopped */
	if ( ! op ) {
		assert ( tlskey->secretsize == 0 );
		assert ( tlskey->kdf.flags == 0 );
		assert ( tlskey->traffic[TLS_CLIENT].phase == NULL );
		assert ( tlskey->traffic[TLS_SERVER].phase == NULL );
		return;
	}

	/* Clear secrets, flags, and traffic phases */
	memset ( tlskey->secret, 0, tlskey->secretsize );
	tlskey->kdf.flags = 0;
	tlskey->traffic[TLS_CLIENT].phase = NULL;
	tlskey->traffic[TLS_SERVER].phase = NULL;

	/* Reset key schedule */
	op->reset ( tlskey );
}

/**
 * Apply a new shared secret
 *
 * @v tlskey		Key schedule
 * @v shared		New shared secret
 * @v shared_len	Length of new shared secret
 * @ret rc		Return status code
 */
int tlskey_apply ( struct tls_key_schedule *tlskey, const void *shared,
		   size_t shared_len ) {
	const struct tls_key_schedule_operations *op = tlskey->op;
	int rc;

	/* Sanity check */
	if ( ! op )
		return -ENOTTY;

	/* All schedules define the master secret as immutable */
	if ( tlskey->kdf.flags & TLSKEY_KDF_MASTER ) {
		DBGC ( tlskey, "TLSKEY %p cannot apply a new shared secret "
		       "after generating master secret\n", tlskey );
		return -EPROTO;
	}

	/* Apply new shared secret */
	DBGC ( tlskey, "TLSKEY %p applying secret:\n", tlskey );
	DBGC_HDA ( tlskey, 0, shared, shared_len );
	if ( ( rc = op->apply ( tlskey, shared, shared_len ) ) != 0 )
		return rc;

	/* Mark key schedule as containing key material */
	tlskey->kdf.flags |= TLSKEY_KDF_KEYED;

	return 0;
}

/**
 * Generate master secret
 *
 * @v tlskey		Key schedule
 * @v ems		Extended master secret extension is enabled
 * @ret rc		Return status code
 */
int tlskey_master ( struct tls_key_schedule *tlskey, int ems ) {
	const struct tls_key_schedule_operations *op = tlskey->op;
	int rc;

	/* Sanity check */
	if ( ! op )
		return -ENOTTY;

	/* Any conceivable schedule requires key material */
	if ( ! ( tlskey->kdf.flags & TLSKEY_KDF_KEYED ) ) {
		DBGC ( tlskey, "TLSKEY %p cannot generate master secret "
		       "without key material\n", tlskey );
		return -EPROTO;
	}

	/* All schedules define the master secret as immutable */
	if ( tlskey->kdf.flags & TLSKEY_KDF_MASTER ) {
		DBGC ( tlskey, "TLSKEY %p cannot regenerate master secret\n",
		       tlskey );
		return -EPROTO;
	}

	/* Generate master secret */
	DBGC ( tlskey, "TLSKEY %p generating %smaster secret\n",
	       tlskey, ( ems ? "extended " : "" ) );
	if ( ( rc = op->master ( tlskey, ems ) ) != 0 )
		return rc;

	/* Mark key schedule as containing master secret */
	tlskey->kdf.flags |= TLSKEY_KDF_MASTER;
	if ( ems )
		tlskey->kdf.flags |= TLSKEY_KDF_EMS;

	return 0;
}

/**
 * Generate verification data
 *
 * @v tlskey		Key schedule
 * @v end		Verification endpoint
 * @v verify		Verification data to fill in
 * @v verify_len	Length of verification data
 * @ret rc		Return status code
 */
int tlskey_verify ( struct tls_key_schedule *tlskey,
		    const struct tls_endpoint *end,
		    void *verify, size_t verify_len ) {
	const struct tls_key_schedule_operations *op = tlskey->op;
	int rc;

	/* Sanity check */
	if ( ! op )
		return -ENOTTY;

	/* Verification data must be non-replayable */
	if ( ! ( tlskey->transcript.flags & TLSKEY_TSF_NONCED ) ) {
		DBGC ( tlskey, "TLSKEY %p cannot generate verification data "
		       "without a digested nonce\n", tlskey );
		return -EPROTO;
	}

	/* Generate verification data */
	DBGC2 ( tlskey, "TLSKEY %p generating %s verification data\n",
		tlskey, end->name );
	if ( ( rc = op->verify ( tlskey, end, verify, verify_len ) ) != 0 )
		return rc;
	DBGC ( tlskey, "TLSKEY %p %s verification data:\n",
	       tlskey, end->name );
	DBGC_HDA ( tlskey, 0, verify, verify_len );

	return 0;
}

/**
 * Generate traffic secret
 *
 * @v tlskey		Key schedule
 * @v writer		Writer endpoint
 * @v phase		Traffic phase
 * @ret rc		Return status code
 */
int tlskey_traffic ( struct tls_key_schedule *tlskey,
		     const struct tls_endpoint *writer,
		     const struct tls_phase *phase ) {
	const struct tls_key_schedule_operations *op = tlskey->op;
	struct tls_traffic_secret *traffic = &tlskey->traffic[writer->index];
	unsigned int missing;
	int rc;

	/* Sanity check */
	if ( ! op )
		return -ENOTTY;

	/* Check flags required for this traffic phase */
	assert ( phase->flags & TLSKEY_KDF_KEYED );
	missing = ( ( tlskey->kdf.flags ^ phase->flags ) & phase->flags );
	if ( missing ) {
		DBGC ( tlskey, "TLSKEY %p cannot generate %s %s traffic "
		       "secrets (missing KDF flags %#04x)\n",
		       tlskey, writer->name, phase->name, missing );
		return -EPROTO;
	}

	/* Generate traffic secret */
	DBGC2 ( tlskey, "TLSKEY %p generating %s %s traffic secret\n",
		tlskey, writer->name, phase->name );
	if ( ( rc = op->traffic ( tlskey, writer, phase ) ) != 0 )
		return rc;

	/* Record phase */
	traffic->phase = phase;

	return 0;
}

/**
 * Generate cipher key material
 *
 * @v tlskey		Key schedule
 * @v writer		Writer endpoint
 * @v key		Cipher key to fill in
 * @v key_len		Length of cipher key
 * @v iv		Fixed portion of initialisation vector to fill in
 * @v iv_len		Length of fixed portion of initialisation vector
 * @v mac		MAC secret to fill in
 * @v mac_len		Length of MAC secret
 * @ret rc		Return status code
 *
 * For key schedules that include a ratchet mechanism, each call
 * consumes the current generation of traffic secret and replaces it
 * with the next generation.  Repeated calls may therefore be used to
 * obtain multiple generations of the cipher key material.
 */
int tlskey_cipher ( struct tls_key_schedule *tlskey,
		    const struct tls_endpoint *writer,
		    void *key, size_t key_len, void *iv, size_t iv_len,
		    void *mac, size_t mac_len ) {
	const struct tls_key_schedule_operations *op = tlskey->op;
	struct tls_traffic_secret *traffic = &tlskey->traffic[writer->index];
	int rc;

	/* Sanity check */
	if ( ! op )
		return -ENOTTY;

	/* Ensure that traffic secret has actually been generated */
	if ( ! traffic->phase ) {
		DBGC ( tlskey, "TLSKEY %p cannot generate cipher key "
		       "material without traffic secrets\n", tlskey );
		return -EPROTO;
	}

	/* Generate cipher key material */
	DBGC2 ( tlskey, "TLSKEY %p generating %s %s cipher key material\n",
		tlskey, writer->name, traffic->phase->name );
	if ( ( rc = op->cipher ( tlskey, writer, key, key_len, iv, iv_len,
				 mac, mac_len ) ) != 0 ) {
		return rc;
	}
	if ( key_len ) {
		DBGC ( tlskey, "TLSKEY %p %s key:\n", tlskey, writer->name );
		DBGC_HDA ( tlskey, 0, key, key_len );
	}
	if ( iv_len ) {
		DBGC ( tlskey, "TLSKEY %p %s IV:\n", tlskey, writer->name );
		DBGC_HDA ( tlskey, 0, iv, iv_len );
	}
	if ( mac_len ) {
		DBGC ( tlskey, "TLSKEY %p %s MAC:\n", tlskey, writer->name );
		DBGC_HDA ( tlskey, 0, mac, mac_len );
	}

	return 0;
}

/**
 * Generate signable digest value
 *
 * @v tlskey		Key schedule
 * @v end		Endpoint
 * @v digest		Signature digest algorithm
 * @v data		Additional data
 * @v len		Length of additional data
 * @v tbs		Signable digest value to fill in
 * @ret rc		Return status code
 */
int tlskey_tbshash ( struct tls_key_schedule *tlskey,
		     const struct tls_endpoint *end,
		     struct digest_algorithm *digest,
		     const void *data, size_t len, void *tbs ) {
	const struct tls_key_schedule_operations *op = tlskey->op;
	int rc;

	/* Sanity check */
	if ( ! op )
		return -ENOTTY;

	/* Signable digest values must be non-replayable */
	if ( ! ( tlskey->transcript.flags & TLSKEY_TSF_NONCED ) ) {
		DBGC ( tlskey, "TLSKEY %p cannot generate signable digest "
		       "without a digested nonce\n", tlskey );
		return -EPROTO;
	}

	/* Generate digest value */
	DBGC2 ( tlskey, "TLSKEY %p generating signable %s %s digest\n",
		tlskey, end->name, digest->name );
	if ( ( rc = op->tbshash ( tlskey, end, digest, data, len,
				  tbs ) ) != 0 ) {
		return rc;
	}
	DBGC ( tlskey, "TLSKEY %p generated signable %s %s digest:\n",
	       tlskey, end->name, digest->name );
	DBGC_HDA ( tlskey, 0, tbs, digest->digestsize );

	return 0;
}

/**
 * Save pre-shared key
 *
 * @v tlskey		Key schedule
 * @v nonce		Ticket nonce
 * @v nonce_len		Length of ticket nonce
 * @v psk		Pre-shared key to fill in
 * @ret rc		Return status code
 */
int tlskey_save ( struct tls_key_schedule *tlskey, const void *nonce,
		  size_t nonce_len, struct tls_preshared_key *psk ) {
	const struct tls_key_schedule_operations *op = tlskey->op;
	struct digest_algorithm *digest = tlskey->digest;
	int rc;

	/* Clear any existing pre-shared key */
	memset ( psk, 0, sizeof ( *psk ) );

	/* Sanity check */
	if ( ! op )
		return -ENOTTY;

	/* Session resumption requires a master secret */
	if ( ! ( tlskey->kdf.flags & TLSKEY_KDF_MASTER ) ) {
		DBGC ( tlskey, "TLSKEY %p cannot save key without a master "
		       "secret\n", tlskey );
		return -EPROTO;
	}

	/* Save key material */
	DBGC2 ( tlskey, "TLSKEY %p saving key\n", tlskey );
	if ( ( rc = op->save ( tlskey, nonce, nonce_len, psk ) ) != 0 )
		return rc;
	DBGC ( tlskey, "TLSKEY %p saved key:\n", tlskey );
	DBGC_HDA ( tlskey, 0, &psk->key, sizeof ( psk->key ) );

	/* Record key properties */
	psk->op = op;
	psk->digest = digest;
	psk->flags = tlskey->kdf.flags;

	return 0;
}

/**
 * Load pre-shared key
 *
 * @v tlskey		Key schedule
 * @v ems		Extended master secret extension is enabled
 * @v psk		Pre-shared key
 * @ret rc		Return status code
 */
int tlskey_load ( struct tls_key_schedule *tlskey, int ems,
		  const struct tls_preshared_key *psk ) {
	const struct tls_key_schedule_operations *op = tlskey->op;
	struct digest_algorithm *digest = tlskey->digest;
	int rc;

	/* Reset key schedule */
	tlskey_reset ( tlskey );

	/* Sanity check */
	if ( ! op )
		return -ENOTTY;

	/* Session resumption requires a master secret */
	if ( ! ( psk->flags & TLSKEY_KDF_MASTER ) ) {
		DBGC ( tlskey, "TLSKEY %p cannot load from a non-master "
		       "secret\n", tlskey );
		return -EPROTO;
	}

	/* Pre-shared key must match extended/non-extended usage */
	if ( ( !! ems ) != ( !! ( psk->flags & TLSKEY_KDF_EMS ) ) ) {
		DBGC ( tlskey, "TLSKEY %p cannot load from %sextended master "
		       "secret\n", tlskey, ( ems ? "non-" : "" ) );
		return -EPERM;
	}

	/* Pre-shared key must match schedule and digest */
	if ( ( op != psk->op ) || ( digest != psk->digest ) ) {
		DBGC ( tlskey, "TLSKEY %p cannot load from %s with %s\n",
		       tlskey, ( psk->op ? psk->op->name : "(unknown)" ),
		       ( psk->digest ? psk->digest->name : "(unknown)" ) );
		return -EPERM;
	}

	/* Load key material */
	DBGC ( tlskey, "TLSKEY %p loading key:\n", tlskey );
	DBGC_HDA ( tlskey, 0, &psk->key, sizeof ( psk->key ) );
	if ( ( rc = op->load ( tlskey, psk ) ) != 0 )
		return rc;

	/* Set flags */
	tlskey->kdf.flags = ( psk->flags & op->mask );

	return 0;
}

/**
 * Generate pre-shared key binder value
 *
 * @v psk		Pre-shared key
 * @v prefix		ClientHello prefix
 * @v prefix_len	Length of ClientHello prefix
 * @v binder		Binder value to fill in
 * @v binder_len	Length of binder value
 * @ret rc		Return status code
 */
int tlskey_bind ( const struct tls_preshared_key *psk, const void *prefix,
		  size_t prefix_len, void *binder, size_t binder_len ) {
	const struct tls_key_schedule_operations *op = psk->op;
	struct digest_algorithm *digest = psk->digest;
	int rc;

	/* Generate prefix digest and binder value */
	if ( op && digest ) {
		size_t digestsize = digest->digestsize;
		size_t ctxsize = digest->ctxsize;
		uint8_t ctx[ctxsize];
		uint8_t out[digestsize];

		/* Calculate prefix digest */
		digest_init ( digest, ctx );
		digest_update ( digest, ctx, prefix, prefix_len );
		digest_final ( digest, ctx, out );

		/* Generate binder value */
		DBGC2 ( psk, "TLSKEY %p generating key binder:\n", psk );
		DBGC2_HDA ( psk, 0, &psk->key, sizeof ( psk->key ) );
		if ( ( rc = op->bind ( psk, out, binder, binder_len ) ) != 0 )
			return rc;
		DBGC ( psk, "TLSKEY %p generated key binder:\n", psk );
		DBGC_HDA ( psk, 0, binder, binder_len );

		return 0;
	}

	DBGC ( psk, "TLSKEY %p cannot bind empty pre-shared key\n", psk );
	return -ENOENT;
}

/*****************************************************************************
 *
 * TLS version 1.3 key schedule using HKDF
 *
 *****************************************************************************
 */

/**
 * Calculate secret size
 *
 * @v digest		Digest algorithm
 * @ret secretsize	Secret size, or zero if unsupported
 */
static size_t tlskey_hkdf_secretsize ( struct digest_algorithm *digest ) {
	size_t digestsize = digest->digestsize;

	return ( digestsize * 3 /* KDF, client, and server secrets */ );
}

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
static void tlskey_hkdf_expand ( struct digest_algorithm *digest,
				 const void *secret, const char *label,
				 const void *seed, size_t seed_len,
				 void *out, size_t out_len ) {
	static const char prefix[6] = "tls13 ";
	const void *context = seed;
	size_t context_len = seed_len;
	size_t prefix_len = sizeof ( prefix );
	size_t label_len = strlen ( label );
	struct {
		uint16_t len;
		uint8_t label_len;
		char prefix[prefix_len];
		char label[label_len];
		uint8_t context_len;
		uint8_t context[context_len];
	} __attribute__ (( packed )) info;

	/* Construct additional information */
	info.len = htons ( out_len );
	info.label_len = ( prefix_len + label_len );
	memcpy ( info.prefix, prefix, prefix_len );
	memcpy ( info.label, label, label_len );
	info.context_len = context_len;
	memcpy ( info.context, context, context_len );

	/* Generate output using HKDF */
	hkdf_expand ( digest, secret, &info, sizeof ( info ), out, out_len );
}

/**
 * Calculate empty digest value
 *
 * @v digest		Digest algorithm
 * @v empty		Empty digest to fill in
 */
static void tlskey_hkdf_empty ( struct digest_algorithm *digest,
				void *empty ) {
	size_t ctxsize = digest->ctxsize;
	uint8_t ctx[ctxsize];

	/* Calculate empty digest */
	digest_init ( digest, ctx );
	digest_final ( digest, ctx, empty );
}

/**
 * Calculate a finished MAC
 *
 * @v digest		Digest algorithm
 * @v secret		Secret
 * @v hash		Message hash
 * @v out		Output buffer
 */
static void tlskey_hkdf_finished ( struct digest_algorithm *digest,
				   const void *secret, const void *hash,
				   void *out ) {
	size_t digestsize = digest->digestsize;
	size_t hctxsize = hmac_ctxsize ( digest );
	struct {
		uint8_t hctx[hctxsize];
		uint8_t key[digestsize];
	} tmp;

	/* Generate HMAC key */
	tlskey_hkdf_expand ( digest, secret, "finished", NULL, 0,
			     tmp.key, sizeof ( tmp.key ) );

	/* Generate HMAC */
	hmac_init ( digest, tmp.hctx, tmp.key, sizeof ( tmp.key ) );
	hmac_update ( digest, tmp.hctx, hash, digestsize );
	hmac_final ( digest, tmp.hctx, out );

	/* Clear temporary secrets */
	memset ( &tmp, 0, sizeof ( tmp ) );
}

/**
 * Reset key schedule
 *
 * @v key		Key schedule
 */
static void tlskey_hkdf_reset ( struct tls_key_schedule *tlskey ) {
	struct digest_algorithm *digest = tlskey->digest;
	size_t digestsize = digest->digestsize;
	void *secret = tlskey->secret;

	/* Allocate and initialise secrets */
	tlskey->kdf.secret = secret;			secret += digestsize;
	tlskey->traffic[TLS_CLIENT].secret = secret;	secret += digestsize;
	tlskey->traffic[TLS_SERVER].secret = secret;	secret += digestsize;
	assert ( secret == ( tlskey->secret + tlskey->secretsize ) );

	/* Initialise empty early secret */
	hkdf_extract ( digest, NULL, 0, tlskey->kdf.secret, digestsize,
		       tlskey->kdf.secret );
}

/**
 * Apply a new shared secret
 *
 * @v tlskey		Key schedule
 * @v shared		New shared secret
 * @v shared_len	Length of new shared secret
 * @ret rc		Return status code
 */
static int tlskey_hkdf_apply ( struct tls_key_schedule *tlskey,
			       const void *shared, size_t shared_len ) {
	struct digest_algorithm *digest = tlskey->digest;
	size_t digestsize = digest->digestsize;
	uint8_t empty[digestsize];

	/* Generate derived secret */
	tlskey_hkdf_empty ( digest, empty );
	tlskey_expand ( tlskey, tlskey->kdf.secret, "derived", empty,
			digestsize, tlskey->kdf.secret, digestsize );
	DBGC2 ( tlskey, "TLSKEY %p derived:\n", tlskey );
	DBGC2_HDA ( tlskey, 0, tlskey->kdf.secret, digestsize );

	/* Extract new pseudorandom key */
	hkdf_extract ( digest, tlskey->kdf.secret, digestsize, shared,
		       shared_len, tlskey->kdf.secret );
	DBGC2 ( tlskey, "TLSKEY %p extracted PRK:\n", tlskey );
	DBGC2_HDA ( tlskey, 0, tlskey->kdf.secret, digestsize );

	return 0;
}

/**
 * Generate master secret
 *
 * @v tlskey		Key schedule
 * @v ems		Extended master secret extension is enabled
 * @ret rc		Return status code
 */
static int tlskey_hkdf_master ( struct tls_key_schedule *tlskey, int ems ) {
	struct digest_algorithm *digest = tlskey->digest;
	size_t digestsize = digest->digestsize;
	uint8_t zero[digestsize];
	int rc;

	/* Extended master secret must always be used */
	if ( ! ems ) {
		DBGC ( tlskey, "TLSKEY %p requires an extended master "
		       "secret\n", tlskey );
		return -EPROTO;
	}

	/* Generate master secret */
	memset ( zero, 0, sizeof ( zero ) );
	if ( ( rc = tlskey_apply ( tlskey, zero, sizeof ( zero ) ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Generate verification data
 *
 * @v tlskey		Key schedule
 * @v end		Verification endpoint
 * @v verify		Verification data to fill in
 * @v verify_len	Length of verification data
 * @ret rc		Return status code
 */
static int tlskey_hkdf_verify ( struct tls_key_schedule *tlskey,
				const struct tls_endpoint *end,
				void *verify, size_t verify_len ) {
	struct tls_traffic_secret *traffic = &tlskey->traffic[end->index];
	struct digest_algorithm *digest = tlskey->digest;
	size_t digestsize = digest->digestsize;

	/* Verification data must be a complete digest output */
	if ( verify_len != digestsize )
		return -EINVAL;

	/* Verification data is derived from handshake secrets */
	if ( traffic->phase != &tls_handshake ) {
		DBGC ( tlskey, "TLSKEY %p cannot generate %s verification "
		       "data without %s handshake traffic secrets\n",
		       tlskey, end->name, end->name );
		return -EPROTO;
	}

	/* Generate verification data */
	tlskey_hkdf_finished ( digest, traffic->secret,
			       tlskey->transcript.running, verify );
	DBGC ( tlskey, "TLSKEY %p %s verification:\n", tlskey, end->name );
	DBGC_HDA ( tlskey, 0, verify, verify_len );

	return 0;
}

/**
 * Generate traffic secret
 *
 * @v tlskey		Key schedule
 * @v writer		Writer endpoint
 * @v phase		Traffic phase
 * @ret rc		Return status code
 */
static int tlskey_hkdf_traffic ( struct tls_key_schedule *tlskey,
				 const struct tls_endpoint *writer,
				 const struct tls_phase *phase ) {
	struct tls_traffic_secret *traffic = &tlskey->traffic[writer->index];
	struct digest_algorithm *digest = tlskey->digest;
	size_t digestsize = digest->digestsize;
	char label[ 13 /* "[c|s] [e|hs|ap] traffic" + NUL */ ];

	/* Generate label */
	snprintf ( label, sizeof ( label ), "%c %s traffic",
		   writer->name[0], phase->label );

	/* Generate traffic secret */
	tlskey_expand ( tlskey, tlskey->kdf.secret, label,
			tlskey->transcript.finishing, digestsize,
			traffic->secret, digestsize );

	return 0;
}

/**
 * Generate cipher key material
 *
 * @v tlskey		Key schedule
 * @v writer		Writer endpoint
 * @v key		Cipher key to fill in
 * @v key_len		Length of cipher key
 * @v iv		Fixed portion of initialisation vector to fill in
 * @v iv_len		Length of fixed portion of initialisation vector
 * @v mac		MAC secret to fill in
 * @v mac_len		Length of MAC secret
 * @ret rc		Return status code
 */
static int tlskey_hkdf_cipher ( struct tls_key_schedule *tlskey,
				 const struct tls_endpoint *writer,
				 void *key, size_t key_len, void *iv,
				 size_t iv_len, void *mac __unused,
				 size_t mac_len ) {
	struct tls_traffic_secret *traffic = &tlskey->traffic[writer->index];
	struct digest_algorithm *digest = tlskey->digest;
	size_t digestsize = digest->digestsize;
	void *secret = traffic->secret;

	/* The key schedule does not define a way to generate MAC secrets */
	if ( mac_len ) {
		DBGC ( tlskey, "TLSKEY %p does not support MAC secrets\n",
		       tlskey );
		return -ENOTSUP;
	}

	/* Generate cipher key */
	tlskey_expand ( tlskey, secret, "key", NULL, 0, key, key_len );

	/* Generate initialisation vector */
	tlskey_expand ( tlskey, secret, "iv", NULL, 0, iv, iv_len );

	/* Update traffic secret, if applicable */
	if ( traffic->phase == &tls_application ) {
		tlskey_expand ( tlskey, secret, "traffic upd", NULL, 0,
				secret, digestsize );
	}

	return 0;
}

/**
 * Generate signable digest value
 *
 * @v tlskey		Key schedule
 * @v end		Endpoint
 * @v digest		Signature digest algorithm
 * @v data		Additional data
 * @v len		Length of additional data
 * @v tbs		Signable digest value to fill in
 * @ret rc		Return status code
 */
static int tlskey_hkdf_tbshash ( struct tls_key_schedule *tlskey,
				 const struct tls_endpoint *end,
				 struct digest_algorithm *digest,
				 const void *data __unused, size_t len,
				 void *tbs ) {
	size_t ctxsize = digest->ctxsize;
	uint8_t ctx[ctxsize];
	char buf[64];
	int label_len;

	/* There is no way to incorporate additional data */
	if ( len ) {
		DBGC ( tlskey, "TLSKEY %p cannot generate digest with "
		       "additional data\n", tlskey );
		return -ENOTSUP;
	}

	/* Generate digest value (all input is public) */
	digest_init ( digest, ctx );
	memset ( buf, 0x20, sizeof ( buf ) );
	digest_update ( digest, ctx, buf, sizeof ( buf ) );
	label_len = snprintf ( buf, sizeof ( buf ),
			       "TLS 1.3, %s CertificateVerify", end->name );
	assert ( label_len == 33 /* for both "client" and "server" */ );
	digest_update ( digest, ctx, buf, ( label_len + 1 /* NUL */ ) );
	digest_update ( digest, ctx, tlskey->transcript.running,
			tlskey->digest->digestsize );
	digest_final ( digest, ctx, tbs );

	return 0;
}

/**
 * Save pre-shared key
 *
 * @v tlskey		Key schedule
 * @v nonce		Ticket nonce
 * @v nonce_len		Length of ticket nonce
 * @v psk		Pre-shared key to fill in
 * @ret rc		Return status code
 */
static int tlskey_hkdf_save ( struct tls_key_schedule *tlskey,
			      const void *nonce, size_t nonce_len,
			      struct tls_preshared_key *psk ) {
	struct digest_algorithm *digest = tlskey->digest;
	size_t digestsize = digest->digestsize;
	struct {
		uint8_t master[digestsize];
	} tmp;

	/* The pre-shared key currently uses a fixed-size buffer */
	if ( digestsize > sizeof ( psk->key.resumption ) ) {
		DBGC ( tlskey, "TLSKEY %p cannot save %s pre-shared key\n",
		       tlskey, digest->name );
		return -ENOTSUP;
	}

	/* Generate resumption master secret */
	tlskey_expand ( tlskey, tlskey->kdf.secret, "res master",
			tlskey->transcript.finishing, digestsize,
			tmp.master, sizeof ( tmp.master ) );

	/* Generate resumption secret */
	tlskey_expand ( tlskey, tmp.master, "resumption", nonce, nonce_len,
			psk->key.resumption, digestsize );

	/* Clear temporary secrets */
	memset ( &tmp, 0, sizeof ( tmp ) );

	return 0;
}

/**
 * Load pre-shared key
 *
 * @v tlskey		Key schedule
 * @v psk		Pre-shared key
 * @ret rc		Return status code
 */
static int tlskey_hkdf_load ( struct tls_key_schedule *tlskey,
			      const struct tls_preshared_key *psk ) {
	struct digest_algorithm *digest = tlskey->digest;
	size_t digestsize = digest->digestsize;

	/* Sanity checks */
	assert ( digest == psk->digest );
	assert ( digestsize <= sizeof ( psk->key.resumption ) );

	/* Extract early secret */
	hkdf_extract ( digest, NULL, 0, &psk->key, digestsize,
		       tlskey->kdf.secret );
	DBGC2 ( tlskey, "TLSKEY %p early secret:\n", tlskey );
	DBGC2_HDA ( tlskey, 0, tlskey->kdf.secret, digestsize );

	return 0;
}

/**
 * Generate pre-shared key binder value
 *
 * @v psk		Pre-shared key
 * @v hash		Partial transcript hash
 * @v binder		Binder value to fill in
 * @v binder_len	Length of binder value
 * @ret rc		Return status code
 */
static int tlskey_hkdf_bind ( const struct tls_preshared_key *psk,
			      const void *hash, void *binder,
			      size_t binder_len ) {
	struct digest_algorithm *digest = psk->digest;
	size_t digestsize = digest->digestsize;
	uint8_t empty[digestsize];
	struct {
		uint8_t key[digestsize];
	} tmp;

	/* Binder value must be a complete digest output */
	if ( binder_len != digestsize )
		return -EINVAL;

	/* Extract temporary copy of early secret */
	hkdf_extract ( digest, NULL, 0, &psk->key, digestsize, tmp.key );
	DBGC2 ( psk, "TLSKEY %p temporary early secret:\n", psk );
	DBGC2_HDA ( psk, 0, tmp.key, sizeof ( tmp.key ) );

	/* Generate resumption binder secret */
	tlskey_hkdf_empty ( digest, empty );
	tlskey_hkdf_expand ( digest, tmp.key, "res binder", empty, digestsize,
			     tmp.key, digestsize );
	DBGC2 ( psk, "TLSKEY %p resumption binder secret:\n", psk );
	DBGC2_HDA ( psk, 0, tmp.key, sizeof ( tmp.key ) );

	/* Generate binder */
	tlskey_hkdf_finished ( digest, tmp.key, hash, binder );
	DBGC2 ( psk, "TLSKEY %p binder:\n", psk );
	DBGC2_HDA ( psk, 0, binder, binder_len );

	/* Clear temporary secrets */
	memset ( &tmp, 0, sizeof ( tmp ) );

	return 0;
}

/** TLS key schedule based on HKDF */
const struct tls_key_schedule_operations tlskey_hkdf = {
	.name = "HKDF",
	.accumulates = 1,
	.mask = TLSKEY_KDF_KEYED,
	.secretsize = tlskey_hkdf_secretsize,
	.expand = tlskey_hkdf_expand,
	.reset = tlskey_hkdf_reset,
	.apply = tlskey_hkdf_apply,
	.master = tlskey_hkdf_master,
	.verify = tlskey_hkdf_verify,
	.traffic = tlskey_hkdf_traffic,
	.cipher = tlskey_hkdf_cipher,
	.tbshash = tlskey_hkdf_tbshash,
	.save = tlskey_hkdf_save,
	.load = tlskey_hkdf_load,
	.bind = tlskey_hkdf_bind,
};

/*****************************************************************************
 *
 * TLS version 1.2 key schedule using PRF based on P_Hash()
 *
 *****************************************************************************
 */

/** Minimum length for verification data */
#define TLSKEY_HASH_VERIFY_MIN 12

/**
 * Calculate secret size
 *
 * @v digest		Digest algorithm
 * @ret secretsize	Secret size, or zero if unsupported
 */
static size_t tlskey_hash_secretsize ( struct digest_algorithm *digest ) {

	/* A P_Hash() key is an HMAC key */
	return hmac_keysize ( digest );
}

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
static void tlskey_hash_expand ( struct digest_algorithm *digest,
				 const void *secret, const char *label,
				 const void *seed, size_t seed_len,
				 void *out, size_t out_len ) {
	size_t digestsize = digest->digestsize;
	size_t hctxsize = hmac_ctxsize ( digest );
	size_t frag_len = digestsize;
	size_t label_len = strlen ( label );
	unsigned int index = 0;
	struct {
		uint8_t ctx[2][hctxsize];
		uint8_t a[digestsize];
		uint8_t frag[digestsize];
	} tmp;

	/* Generate as much output as required */
	while ( out_len ) {

		/* Generate A(n) and output fragment */
		hmac_init_key ( digest, tmp.ctx[0], secret );
		if ( index ) {
			hmac_update ( digest, tmp.ctx[0], tmp.a,
				      sizeof ( tmp.a ) );
			memcpy ( tmp.ctx[1], tmp.ctx[0],
				 sizeof ( tmp.ctx[1] ) );
		}
		hmac_update ( digest, tmp.ctx[0], label, label_len );
		hmac_update ( digest, tmp.ctx[0], seed, seed_len );
		hmac_final ( digest, tmp.ctx[ ( index != 0 ) ], tmp.a );
		if ( index++ == 0 )
			continue;
		hmac_final ( digest, tmp.ctx[0], tmp.frag );

		/* Copy output */
		if ( frag_len > out_len )
			frag_len = out_len;
		memcpy ( out, tmp.frag, frag_len );

		/* Move to next fragment */
		out += frag_len;
		out_len -= frag_len;
	}

	/* Clear temporary secrets */
	memset ( &tmp, 0, sizeof ( tmp ) );
}

/**
 * Reset key schedule
 *
 * @v key		Key schedule
 */
static void tlskey_hash_reset ( struct tls_key_schedule *tlskey ) {
	void *secret = tlskey->secret;

	/* Allocate and initialise secrets */
	tlskey->kdf.secret = secret;
	tlskey->traffic[TLS_CLIENT].secret = secret;
	tlskey->traffic[TLS_SERVER].secret = secret;
}

/**
 * Apply a new shared secret
 *
 * @v tlskey		Key schedule
 * @v shared		New shared secret
 * @v shared_len	Length of new shared secret
 * @ret rc		Return status code
 */
static int tlskey_hash_apply ( struct tls_key_schedule *tlskey,
			       const void *shared, size_t shared_len ) {
	struct digest_algorithm *digest = tlskey->digest;
	size_t hctxsize = hmac_ctxsize ( digest );
	struct {
		uint8_t hctx[hctxsize];
	} tmp;

	/* Set HMAC key */
	hmac_key ( digest, tmp.hctx, shared, shared_len, tlskey->kdf.secret );

	/* Clear temporary secrets */
	memset ( &tmp, 0, sizeof ( tmp ) );

	return 0;
}

/**
 * Generate master secret
 *
 * @v tlskey		Key schedule
 * @v ems		Extended master secret extension is enabled
 * @ret rc		Return status code
 */
static int tlskey_hash_master ( struct tls_key_schedule *tlskey, int ems ) {
	struct digest_algorithm *digest = tlskey->digest;
	size_t digestsize = digest->digestsize;
	const char *label;
	const void *seed;
	size_t seed_len;
	struct {
		uint8_t master[48];
	} tmp;
	int rc;

	/* Master secret is derived from the client and server random
	 * values (or the full transcript digest).
	 */
	if ( ! ( tlskey->transcript.flags & TLSKEY_TSF_NONCED ) ) {
		DBGC ( tlskey, "TLSKEY %p cannot generate master secret "
		       "without a digested nonce\n", tlskey );
		return -EPROTO;
	}

	/* Generate master secret */
	if ( ems ) {
		label = "extended master secret";
		seed = tlskey->transcript.running;
		seed_len = digestsize;
	} else {
		label = "master secret";
		seed = &tlskey->random;
		seed_len = sizeof ( tlskey->random );
	}
	tlskey_expand ( tlskey, tlskey->kdf.secret, label, seed, seed_len,
			tmp.master, sizeof ( tmp.master ) );

	/* Apply master secret */
	if ( ( rc = tlskey_apply ( tlskey, tmp.master,
				   sizeof ( tmp.master ) ) ) != 0 ) {
		goto err_apply;
	}

 err_apply:
	memset ( &tmp, 0, sizeof ( tmp ) );
	return rc;
}

/**
 * Generate verification data
 *
 * @v tlskey		Key schedule
 * @v end		Verification endpoint
 * @v verify		Verification data to fill in
 * @v verify_len	Length of verification data
 * @ret rc		Return status code
 */
static int tlskey_hash_verify ( struct tls_key_schedule *tlskey,
				const struct tls_endpoint *end,
				void *verify, size_t verify_len ) {
	struct digest_algorithm *digest = tlskey->digest;
	size_t digestsize = digest->digestsize;
	char label[ 16 /* "[client|server] finished" + NUL */ ];

	/* Verification data must be at least 12 bytes for all digests */
	if ( verify_len < TLSKEY_HASH_VERIFY_MIN )
		return -EINVAL;

	/* Verification data is derived from the master secret */
	if ( ! ( tlskey->kdf.flags & TLSKEY_KDF_MASTER ) ) {
		DBGC ( tlskey, "TLSKEY %p cannot generate verification data "
		       "without a master secret\n", tlskey );
		return -EPROTO;
	}

	/* Construct label */
	snprintf ( label, sizeof ( label ), "%s finished", end->name );

	/* Generate verification data */
	tlskey_expand ( tlskey, tlskey->kdf.secret, label,
			tlskey->transcript.running, digestsize,
			verify, verify_len );

	return 0;
}

/**
 * Generate traffic secret
 *
 * @v tlskey		Key schedule
 * @v writer		Writer endpoint
 * @v phase		Traffic phase
 * @ret rc		Return status code
 */
static int tlskey_hash_traffic ( struct tls_key_schedule *tlskey,
				 const struct tls_endpoint *writer,
				 const struct tls_phase *phase ) {
	struct tls_traffic_secret *traffic = &tlskey->traffic[writer->index];

	/* Only application phase keys are supported */
	if ( phase != &tls_application ) {
		DBGC ( tlskey, "TLSKEY %p does not support %s traffic keys\n",
		       tlskey, phase->name );
		return -ENOTSUP;
	}

	/* There are no separate traffic secrets */
	assert ( traffic->secret == tlskey->kdf.secret );

	return 0;
}

/**
 * Generate cipher key material
 *
 * @v tlskey		Key schedule
 * @v writer		Writer endpoint
 * @v key		Cipher key to fill in
 * @v key_len		Length of cipher key
 * @v iv		Fixed portion of initialisation vector to fill in
 * @v iv_len		Length of fixed portion of initialisation vector
 * @v mac		MAC secret to fill in
 * @v mac_len		Length of MAC secret
 * @ret rc		Return status code
 */
static int tlskey_hash_cipher ( struct tls_key_schedule *tlskey,
				const struct tls_endpoint *writer,
				void *key, size_t key_len, void *iv,
				size_t iv_len, void *mac, size_t mac_len ) {
	size_t total = ( 2 * ( mac_len + key_len + iv_len ) );
	struct {
		uint8_t key[total];
	} tmp;
	struct {
		struct tls_random server;
		struct tls_random client;
	} __attribute__ (( packed )) seed;
	const void *src;
	size_t mask;

	/* Construct seed (with swapped client/server random bytes) */
	memcpy ( &seed.server, &tlskey->random[TLS_SERVER],
		 sizeof ( seed.server ) );
	memcpy ( &seed.client, &tlskey->random[TLS_CLIENT],
		 sizeof ( seed.client ) );

	/* Generate key material */
	tlskey_expand ( tlskey, tlskey->kdf.secret, "key expansion", &seed,
			sizeof ( seed ), tmp.key, sizeof ( tmp.key ) );

	/* Partition key material */
	src = tmp.key;
	mask = ( ( writer->index == TLS_CLIENT ) ? 0 : -1UL );
	memcpy ( mac, ( src + ( mac_len & mask ) ), mac_len );
	src += ( 2 * mac_len );
	memcpy ( key, ( src + ( key_len & mask ) ), key_len );
	src += ( 2 * key_len );
	memcpy ( iv, ( src + ( iv_len & mask ) ), iv_len );
	src += ( 2 * iv_len );
	assert ( src == &tmp.key[total] );

	/* Clear temporary secrets */
	memset ( &tmp, 0, sizeof ( tmp ) );

	return 0;
}

/**
 * Generate signable digest value
 *
 * @v tlskey		Key schedule
 * @v end		Endpoint
 * @v digest		Signature digest algorithm
 * @v data		Additional data
 * @v len		Length of additional data
 * @v tbs		Signable digest value to fill in
 * @ret rc		Return status code
 */
static int tlskey_hash_tbshash ( struct tls_key_schedule *tlskey,
				 const struct tls_endpoint *end,
				 struct digest_algorithm *digest,
				 const void *data, size_t len, void *tbs ) {
	size_t ctxsize = digest->ctxsize;
	struct {
		uint8_t ctx[ctxsize];
	} tmp;

	/* Generate endpoint-specific digest value */
	if ( end->index == TLS_CLIENT ) {

		/* The client CertificateVerify digest value is the
		 * raw transcript digest.  We retain only a single
		 * running transcript digest, and can therefore
		 * provide this only for a digest algorithm that
		 * matches our transcript digest algorithm.
		 */
		if ( digest != tlskey->digest ) {
			DBGC ( tlskey, "TLSKEY %p cannot generate %s "
			       "transcript digest\n", tlskey, digest->name );
			return -ENOTSUP;
		}

		/* There is no way to incorporate additional data */
		if ( len ) {
			DBGC ( tlskey, "TLSKEY %p cannot generate digest "
			       "with additional data\n", tlskey );
			return -ENOTSUP;
		}

		/* Copy transcript digest value */
		memcpy ( tbs, tlskey->transcript.running,
			 digest->digestsize );

	} else {

		/* Generate ServerKeyExchange digest */
		digest_init ( digest, tmp.ctx );
		digest_update ( digest, tmp.ctx, &tlskey->random,
				sizeof ( tlskey->random ) );
		digest_update ( digest, tmp.ctx, data, len );
		digest_final ( digest, tmp.ctx, tbs );
	}

	/* Clear temporary secrets */
	memset ( &tmp, 0, sizeof ( tmp ) );

	return 0;
}

/**
 * Save pre-shared key
 *
 * @v tlskey		Key schedule
 * @v nonce		Ticket nonce
 * @v nonce_len		Length of ticket nonce
 * @v psk		Pre-shared key to fill in
 * @ret rc		Return status code
 */
static int tlskey_hash_save ( struct tls_key_schedule *tlskey,
			      const void *nonce __unused, size_t nonce_len,
			      struct tls_preshared_key *psk ) {
	struct digest_algorithm *digest = tlskey->digest;

	/* A ticket nonce is not supported */
	if ( nonce_len )
		return -ENOTSUP;

	/* For TLS version 1.2, the pre-master secret may be any
	 * length but the master secret is fixed at 48 bytes.  This is
	 * smaller than the block size for all supported digest
	 * algorithms.  The HMAC key constructed from the master
	 * secret will therefore be just the zero-padded master secret
	 * value.  We can therefore preserve just these first 48 bytes
	 * of the KDF master secret (ignoring the zero padding up to
	 * the digest block size).
	 */
	assert ( sizeof ( psk->key.master_secret ) <=
		 hmac_keysize ( digest ) );
	memcpy ( &psk->key.master_secret, tlskey->kdf.secret,
		 sizeof ( psk->key.master_secret ) );

	return 0;
}

/**
 * Load pre-shared key
 *
 * @v tlskey		Key schedule
 * @v psk		Pre-shared key
 * @ret rc		Return status code
 */
static int tlskey_hash_load ( struct tls_key_schedule *tlskey,
			      const struct tls_preshared_key *psk ) {
	const void *secret;
	size_t secret_len;
	int rc;

	/* For TLS versions 1.2 and earlier, the pre-shared key
	 * material contains the master secret and so we just apply
	 * this as the key derivation function secret.
	 */
	secret = &psk->key.master_secret;
	secret_len = sizeof ( psk->key.master_secret );
	if ( ( rc = tlskey_apply ( tlskey, secret, secret_len ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Generate pre-shared key binder value
 *
 * @v psk		Pre-shared key
 * @v hash		Partial transcript hash
 * @v binder		Binder value to fill in
 * @v binder_len	Length of binder value
 * @ret rc		Return status code
 */
static int tlskey_hash_bind ( const struct tls_preshared_key *psk __unused,
			      const void *hash __unused,
			      void *binder __unused, size_t binder_len ) {

	/* Binder values are not supported */
	if ( binder_len )
		return -ENOTSUP;

	return 0;
}

/** TLS key schedule based on P_Hash() */
const struct tls_key_schedule_operations tlskey_hash = {
	.name = "P_Hash()",
	.accumulates = 0,
	.mask = ( TLSKEY_KDF_KEYED | TLSKEY_KDF_MASTER | TLSKEY_KDF_EMS ),
	.secretsize = tlskey_hash_secretsize,
	.expand = tlskey_hash_expand,
	.reset = tlskey_hash_reset,
	.apply = tlskey_hash_apply,
	.master = tlskey_hash_master,
	.verify = tlskey_hash_verify,
	.traffic = tlskey_hash_traffic,
	.cipher = tlskey_hash_cipher,
	.tbshash = tlskey_hash_tbshash,
	.save = tlskey_hash_save,
	.load = tlskey_hash_load,
	.bind = tlskey_hash_bind,
};

/*****************************************************************************
 *
 * TLS version 1.0/1.1 key schedule using PRF based on P_MD5()+P_SHA1()
 *
 *****************************************************************************
 */

/**
 * Calculate secret size
 *
 * @v digest		Digest algorithm
 * @ret secretsize	Secret size, or zero if unsupported
 */
static size_t tlskey_md5_sha1_secretsize ( struct digest_algorithm *digest ) {
	struct md5_sha1_hmac_keys *hkeys;
	size_t secretsize;

	/* P_MD5()+P_SHA1() can be used only with the MD5+SHA1 algorithm */
	if ( digest != &md5_sha1_algorithm )
		return 0;

	/* A P_MD5()+P_SHA1() key is a split HMAC-MD5 and HMAC-SHA1 key */
	assert ( sizeof ( hkeys->md5 ) == hmac_keysize ( &md5_algorithm ) );
	assert ( sizeof ( hkeys->sha1 ) == hmac_keysize ( &sha1_algorithm ) );
	secretsize = sizeof ( *hkeys );

	return secretsize;
}

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
static void tlskey_md5_sha1_expand ( struct digest_algorithm *digest,
				     const void *secret, const char *label,
				     const void *seed, size_t seed_len,
				     void *out, size_t out_len ) {
	const struct md5_sha1_hmac_keys *hkeys = secret;
	struct {
		uint8_t sha1[out_len];
	} tmp;
	uint8_t *xor;
	unsigned int i;

	/* P_MD5()+P_SHA1() can be used only with the MD5+SHA1 algorithm */
	assert ( digest == &md5_sha1_algorithm );

	/* Generate MD5 portion into output buffer */
	tlskey_hash_expand ( &md5_algorithm, hkeys->md5, label, seed,
			     seed_len, out, out_len );

	/* Generate SHA-1 portion into temporary buffer */
	tlskey_hash_expand ( &sha1_algorithm, hkeys->sha1, label, seed,
			     seed_len, tmp.sha1, out_len );

	/* XOR together into output buffer */
	xor = out;
	for ( i = 0 ; i < out_len ; i++ )
		xor[i] ^= tmp.sha1[i];

	/* Clear temporary secrets */
	memset ( &tmp, 0, sizeof ( tmp ) );
}

/**
 * Apply a new shared secret
 *
 * @v tlskey		Key schedule
 * @v shared		New shared secret
 * @v shared_len	Length of new shared secret
 * @ret rc		Return status code
 */
static int tlskey_md5_sha1_apply ( struct tls_key_schedule *tlskey,
				   const void *shared, size_t shared_len ) {
	struct digest_algorithm *digest = tlskey->digest;
	struct md5_sha1_hmac_keys *hkeys = tlskey->kdf.secret;
	size_t hctxsize = hmac_ctxsize ( digest );
	struct {
		uint8_t hctx[hctxsize];
	} tmp;
	const void *shared_md5;
	const void *shared_sha1;
	size_t shared_sublen;

	/* P_MD5()+P_SHA1() can be used only with the MD5+SHA1 algorithm */
	assert ( digest == &md5_sha1_algorithm );

	/* Split secret into two, with an overlap of up to one byte */
	shared_sublen = ( ( shared_len + 1 ) / 2 );
	shared_md5 = shared;
	shared_sha1 = ( shared + shared_len - shared_sublen );

	/* Set HMAC-MD5 key */
	assert ( sizeof ( tmp.hctx ) >= hmac_ctxsize ( &md5_algorithm ) );
	hmac_key ( &md5_algorithm, tmp.hctx, shared_md5, shared_sublen,
		   hkeys->md5 );

	/* Set HMAC-SHA1 key */
	assert ( sizeof ( tmp.hctx ) >= hmac_ctxsize ( &sha1_algorithm ) );
	hmac_key ( &sha1_algorithm, tmp.hctx, shared_sha1, shared_sublen,
		   hkeys->sha1 );

	/* Clear temporary secrets */
	memset ( &tmp, 0, sizeof ( tmp ) );

	return 0;
}

/**
 * Save pre-shared key
 *
 * @v tlskey		Key schedule
 * @v nonce		Ticket nonce
 * @v nonce_len		Length of ticket nonce
 * @v psk		Pre-shared key
 * @ret rc		Return status code
 */
static int tlskey_md5_sha1_save ( struct tls_key_schedule *tlskey,
				  const void *nonce __unused,
				  size_t nonce_len,
				  struct tls_preshared_key *psk ) {
	struct digest_algorithm *digest = tlskey->digest;
	struct md5_sha1_hmac_keys *hkeys = tlskey->kdf.secret;

	/* P_MD5()+P_SHA1() can be used only with the MD5+SHA1 algorithm */
	assert ( digest == &md5_sha1_algorithm );

	/* A ticket nonce is not supported */
	if ( nonce_len )
		return -ENOTSUP;

	/* For TLS versions 1.1 and earlier, the pre-master secret may
	 * be any length but the master secret is fixed at 48 bytes.
	 * It will have been split to become 24 bytes in each of the
	 * MD5 and SHA-1 HMAC keys.
	 */
	assert ( sizeof ( psk->key.master_secret.md5 ) <=
		 hmac_keysize ( &md5_algorithm ) );
	assert ( sizeof ( psk->key.master_secret.sha1 ) <=
		 hmac_keysize ( &sha1_algorithm ) );
	memcpy ( psk->key.master_secret.md5, hkeys->md5,
		 sizeof ( psk->key.master_secret.md5 ) );
	memcpy ( psk->key.master_secret.sha1, hkeys->sha1,
		 sizeof ( psk->key.master_secret.sha1 ) );

	return 0;
}

/** TLS key schedule based on P_MD5()+P_SHA1() */
const struct tls_key_schedule_operations tlskey_md5_sha1 = {
	.name = "P_MD5()+P_SHA1()",
	.accumulates = 0,
	.mask = ( TLSKEY_KDF_KEYED | TLSKEY_KDF_MASTER | TLSKEY_KDF_EMS ),
	.secretsize = tlskey_md5_sha1_secretsize,
	.expand = tlskey_md5_sha1_expand,
	.reset = tlskey_hash_reset,
	.apply = tlskey_md5_sha1_apply,
	.master = tlskey_hash_master,
	.verify = tlskey_hash_verify,
	.traffic = tlskey_hash_traffic,
	.cipher = tlskey_hash_cipher,
	.tbshash = tlskey_hash_tbshash,
	.save = tlskey_md5_sha1_save,
	.load = tlskey_hash_load,
	.bind = tlskey_hash_bind,
};
