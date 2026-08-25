/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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
 */

FILE_LICENCE ( GPL2_OR_LATER );
FILE_SECBOOT ( PERMITTED );

/**
 * @file
 *
 * Transport Layer Security Protocol
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/pending.h>
#include <ipxe/hmac.h>
#include <ipxe/md5_sha1.h>
#include <ipxe/iobuf.h>
#include <ipxe/xfer.h>
#include <ipxe/open.h>
#include <ipxe/x509.h>
#include <ipxe/privkey.h>
#include <ipxe/certstore.h>
#include <ipxe/rootcert.h>
#include <ipxe/validator.h>
#include <ipxe/job.h>
#include <ipxe/ffdhe.h>
#include <ipxe/tls.h>
#include <config/crypto.h>

/* Disambiguate the various error causes */
#define EINVAL_CHANGE_CIPHER __einfo_error ( EINFO_EINVAL_CHANGE_CIPHER )
#define EINFO_EINVAL_CHANGE_CIPHER					\
	__einfo_uniqify ( EINFO_EINVAL, 0x01,				\
			  "Invalid Change Cipher record" )
#define EINVAL_ALERT __einfo_error ( EINFO_EINVAL_ALERT )
#define EINFO_EINVAL_ALERT						\
	__einfo_uniqify ( EINFO_EINVAL, 0x02,				\
			  "Invalid Alert record" )
#define EINVAL_HELLO __einfo_error ( EINFO_EINVAL_HELLO )
#define EINFO_EINVAL_HELLO						\
	__einfo_uniqify ( EINFO_EINVAL, 0x03,				\
			  "Invalid Server Hello record" )
#define EINVAL_CERTIFICATE __einfo_error ( EINFO_EINVAL_CERTIFICATE )
#define EINFO_EINVAL_CERTIFICATE					\
	__einfo_uniqify ( EINFO_EINVAL, 0x04,				\
			  "Invalid Certificate" )
#define EINVAL_CERTIFICATES __einfo_error ( EINFO_EINVAL_CERTIFICATES )
#define EINFO_EINVAL_CERTIFICATES					\
	__einfo_uniqify ( EINFO_EINVAL, 0x05,				\
			  "Invalid Server Certificate record" )
#define EINVAL_HELLO_DONE __einfo_error ( EINFO_EINVAL_HELLO_DONE )
#define EINFO_EINVAL_HELLO_DONE						\
	__einfo_uniqify ( EINFO_EINVAL, 0x06,				\
			  "Invalid Server Hello Done record" )
#define EINVAL_FINISHED __einfo_error ( EINFO_EINVAL_FINISHED )
#define EINFO_EINVAL_FINISHED						\
	__einfo_uniqify ( EINFO_EINVAL, 0x07,				\
			  "Invalid Server Finished record" )
#define EINVAL_HANDSHAKE __einfo_error ( EINFO_EINVAL_HANDSHAKE )
#define EINFO_EINVAL_HANDSHAKE						\
	__einfo_uniqify ( EINFO_EINVAL, 0x08,				\
			  "Invalid Handshake record" )
#define EINVAL_IV __einfo_error ( EINFO_EINVAL_IV )
#define EINFO_EINVAL_IV							\
	__einfo_uniqify ( EINFO_EINVAL, 0x0a,				\
			  "Invalid initialisation vector" )
#define EINVAL_PADDING __einfo_error ( EINFO_EINVAL_PADDING )
#define EINFO_EINVAL_PADDING						\
	__einfo_uniqify ( EINFO_EINVAL, 0x0b,				\
			  "Invalid block padding" )
#define EINVAL_RX_STATE __einfo_error ( EINFO_EINVAL_RX_STATE )
#define EINFO_EINVAL_RX_STATE						\
	__einfo_uniqify ( EINFO_EINVAL, 0x0c,				\
			  "Invalid receive state" )
#define EINVAL_MAC __einfo_error ( EINFO_EINVAL_MAC )
#define EINFO_EINVAL_MAC						\
	__einfo_uniqify ( EINFO_EINVAL, 0x0d,				\
			  "Invalid MAC or authentication tag" )
#define EINVAL_TICKET __einfo_error ( EINFO_EINVAL_TICKET )
#define EINFO_EINVAL_TICKET						\
	__einfo_uniqify ( EINFO_EINVAL, 0x0e,				\
			  "Invalid New Session Ticket record")
#define EINVAL_KEY_EXCHANGE __einfo_error ( EINFO_EINVAL_KEY_EXCHANGE )
#define EINFO_EINVAL_KEY_EXCHANGE					\
	__einfo_uniqify ( EINFO_EINVAL, 0x0f,				\
			  "Invalid Server Key Exchange record" )
#define EIO_ALERT __einfo_error ( EINFO_EIO_ALERT )
#define EINFO_EIO_ALERT							\
	__einfo_uniqify ( EINFO_EIO, 0x01,				\
			  "Unknown alert level" )
#define ENOENT_CERT __einfo_error ( EINFO_ENOENT_CERT )
#define EINFO_ENOENT_CERT						\
	__einfo_uniqify ( EINFO_ENOENT, 0x01,				\
			  "Missing server certificate" )
#define ENOENT_KEY_EXCHANGE __einfo_error ( EINFO_ENOENT_KEY_EXCHANGE )
#define EINFO_ENOENT_KEY_EXCHANGE					\
	__einfo_uniqify ( EINFO_ENOENT, 0x02,				\
			  "No key exchange algorithm selected" )
#define ENOMEM_CONTEXT __einfo_error ( EINFO_ENOMEM_CONTEXT )
#define EINFO_ENOMEM_CONTEXT						\
	__einfo_uniqify ( EINFO_ENOMEM, 0x01,				\
			  "Not enough space for crypto context" )
#define ENOMEM_CERTIFICATE __einfo_error ( EINFO_ENOMEM_CERTIFICATE )
#define EINFO_ENOMEM_CERTIFICATE					\
	__einfo_uniqify ( EINFO_ENOMEM, 0x02,				\
			  "Not enough space for certificate" )
#define ENOMEM_CHAIN __einfo_error ( EINFO_ENOMEM_CHAIN )
#define EINFO_ENOMEM_CHAIN						\
	__einfo_uniqify ( EINFO_ENOMEM, 0x03,				\
			  "Not enough space for certificate chain" )
#define ENOMEM_TX_PLAINTEXT __einfo_error ( EINFO_ENOMEM_TX_PLAINTEXT )
#define EINFO_ENOMEM_TX_PLAINTEXT					\
	__einfo_uniqify ( EINFO_ENOMEM, 0x04,				\
			  "Not enough space for transmitted plaintext" )
#define ENOMEM_TX_CIPHERTEXT __einfo_error ( EINFO_ENOMEM_TX_CIPHERTEXT )
#define EINFO_ENOMEM_TX_CIPHERTEXT					\
	__einfo_uniqify ( EINFO_ENOMEM, 0x05,				\
			  "Not enough space for transmitted ciphertext" )
#define ENOMEM_RX_DATA __einfo_error ( EINFO_ENOMEM_RX_DATA )
#define EINFO_ENOMEM_RX_DATA						\
	__einfo_uniqify ( EINFO_ENOMEM, 0x07,				\
			  "Not enough space for received data" )
#define ENOMEM_RX_CONCAT __einfo_error ( EINFO_ENOMEM_RX_CONCAT )
#define EINFO_ENOMEM_RX_CONCAT						\
	__einfo_uniqify ( EINFO_ENOMEM, 0x08,				\
			  "Not enough space to concatenate received data" )
#define ENOTSUP_CIPHER __einfo_error ( EINFO_ENOTSUP_CIPHER )
#define EINFO_ENOTSUP_CIPHER						\
	__einfo_uniqify ( EINFO_ENOTSUP, 0x01,				\
			  "Unsupported cipher" )
#define ENOTSUP_NULL __einfo_error ( EINFO_ENOTSUP_NULL )
#define EINFO_ENOTSUP_NULL						\
	__einfo_uniqify ( EINFO_ENOTSUP, 0x02,				\
			  "Refusing to use null cipher" )
#define ENOTSUP_SIG_HASH __einfo_error ( EINFO_ENOTSUP_SIG_HASH )
#define EINFO_ENOTSUP_SIG_HASH						\
	__einfo_uniqify ( EINFO_ENOTSUP, 0x03,				\
			  "Unsupported signature and hash algorithm" )
#define ENOTSUP_VERSION __einfo_error ( EINFO_ENOTSUP_VERSION )
#define EINFO_ENOTSUP_VERSION						\
	__einfo_uniqify ( EINFO_ENOTSUP, 0x04,				\
			  "Unsupported protocol version" )
#define ENOTSUP_GROUP __einfo_error ( EINFO_ENOTSUP_GROUP )
#define EINFO_ENOTSUP_GROUP						\
	__einfo_uniqify ( EINFO_ENOTSUP, 0x05,				\
			  "Unsupported key exchange group" )
#define EPERM_ALERT __einfo_error ( EINFO_EPERM_ALERT )
#define EINFO_EPERM_ALERT						\
	__einfo_uniqify ( EINFO_EPERM, 0x01,				\
			  "Received fatal alert" )
#define EPERM_VERIFY __einfo_error ( EINFO_EPERM_VERIFY )
#define EINFO_EPERM_VERIFY						\
	__einfo_uniqify ( EINFO_EPERM, 0x02,				\
			  "Handshake verification failed" )
#define EPERM_RENEG_INSECURE __einfo_error ( EINFO_EPERM_RENEG_INSECURE )
#define EINFO_EPERM_RENEG_INSECURE					\
	__einfo_uniqify ( EINFO_EPERM, 0x04,				\
			  "Secure renegotiation not supported" )
#define EPERM_RENEG_VERIFY __einfo_error ( EINFO_EPERM_RENEG_VERIFY )
#define EINFO_EPERM_RENEG_VERIFY					\
	__einfo_uniqify ( EINFO_EPERM, 0x05,				\
			  "Secure renegotiation verification failed" )
#define EPERM_KEY_EXCHANGE __einfo_error ( EINFO_EPERM_KEY_EXCHANGE )
#define EINFO_EPERM_KEY_EXCHANGE					\
	__einfo_uniqify ( EINFO_EPERM, 0x06,				\
			  "ServerKeyExchange verification failed" )
#define EPERM_SAVE __einfo_error ( EINFO_EPERM_SAVE )
#define EINFO_EPERM_SAVE						\
	__einfo_uniqify ( EINFO_EPERM, 0x07,				\
			  "Pre-shared key was not established" )
#define EPROTO_VERSION __einfo_error ( EINFO_EPROTO_VERSION )
#define EINFO_EPROTO_VERSION						\
	__einfo_uniqify ( EINFO_EPROTO, 0x01,				\
			  "Illegal protocol version upgrade" )

/** List of TLS session */
static LIST_HEAD ( tls_sessions );

static void tls_tx_resume_all ( struct tls_session *session );
static struct io_buffer * tls_alloc_iob ( struct tls_connection *tls,
					  size_t len );
static int tls_send_alert ( struct tls_connection *tls, unsigned int level,
			    unsigned int description );
static int tls_send_record ( struct tls_connection *tls, unsigned int type,
			     struct io_buffer *iobuf );
static int tls_send_plaintext ( struct tls_connection *tls, unsigned int type,
				const void *data, size_t len );
static void tls_clear_digest ( struct tls_connection *tls );
static void tls_clear_cipher ( struct tls_connection *tls,
			       struct tls_cipherspec *cipherspec );

/******************************************************************************
 *
 * Utility functions
 *
 ******************************************************************************
 */

/** A TLS 24-bit integer
 *
 * TLS uses 24-bit integers in several places, which are awkward to
 * parse in C.
 */
typedef struct {
	/** High byte */
	uint8_t high;
	/** Low word */
	uint16_t low;
} __attribute__ (( packed )) tls24_t;

/**
 * Extract 24-bit field value
 *
 * @v field24		24-bit field
 * @ret value		Field value
 *
 */
static inline __attribute__ (( always_inline )) unsigned long
tls_uint24 ( const tls24_t *field24 ) {

	return ( ( field24->high << 16 ) | be16_to_cpu ( field24->low ) );
}

/**
 * Set 24-bit field value
 *
 * @v field24		24-bit field
 * @v value		Field value
 */
static void tls_set_uint24 ( tls24_t *field24, unsigned long value ) {

	field24->high = ( value >> 16 );
	field24->low = cpu_to_be16 ( value );
}

/**
 * Determine if TLS connection is ready for application data
 *
 * @v tls		TLS connection
 * @ret is_ready	TLS connection is ready
 */
static int tls_ready ( struct tls_connection *tls ) {

	return channel_is_established ( &tls->channel );
}

/**
 * Check for TLS version
 *
 * @v tls		TLS connection
 * @v version		TLS version
 * @ret at_least	TLS connection is using at least the specified version
 *
 * Check that TLS connection uses at least the specified protocol
 * version.  Optimise down to a compile-time constant true result if
 * this is already guaranteed by the minimum supported version check.
 */
static inline __attribute__ (( always_inline )) int
tls_version ( struct tls_connection *tls, unsigned int version ) {
	return ( ( TLS_VERSION_MIN >= version ) ||
		 ( tls->version >= version ) );
}

/**
 * Get pipe name (for debugging)
 *
 * @v tls		TLS connection
 * @v pipe		Secure pipe
 * @ret name		Secure pipe name
 */
static const char * tls_pipe_name ( struct tls_connection *tls,
				    struct secure_pipe *pipe ) {

	if ( pipe == &tls->channel.tx ) {
		return "TX";
	} else if ( pipe == &tls->channel.rx ) {
		return "RX";
	} else {
		return "<UNKNOWN>";
	}
}

/******************************************************************************
 *
 * Cleanup functions
 *
 ******************************************************************************
 */

/**
 * Free TLS session
 *
 * @v refcnt		Reference counter
 */
static void free_tls_session ( struct refcnt *refcnt ) {
	struct tls_session *session =
		container_of ( refcnt, struct tls_session, refcnt );

	/* Sanity check */
	assert ( list_empty ( &session->conn ) );

	/* Remove from list of sessions */
	list_del ( &session->list );

	/* Clear pre-shared identity */
	channel_clear_preshared ( &session->psid );

	/* Free dynamically-allocated resources */
	x509_root_put ( session->root );
	privkey_put ( session->key );
	zfree ( session->ticket.data );

	/* Free session */
	zfree ( session );
}

/**
 * Free TLS connection
 *
 * @v refcnt		Reference counter
 */
static void free_tls ( struct refcnt *refcnt ) {
	struct tls_connection *tls =
		container_of ( refcnt, struct tls_connection, refcnt );
	struct tls_session *session = tls->session;
	struct io_buffer *iobuf;
	struct io_buffer *tmp;

	/* Free dynamically-allocated resources */
	zfree ( tls->new_ticket.data );
	tls_clear_digest ( tls );
	tls_clear_cipher ( tls, &tls->tx.cipherspec.active );
	tls_clear_cipher ( tls, &tls->tx.cipherspec.pending );
	tls_clear_cipher ( tls, &tls->rx.cipherspec.active );
	tls_clear_cipher ( tls, &tls->rx.cipherspec.pending );
	list_for_each_entry_safe ( iobuf, tmp, &tls->rx.data, list ) {
		list_del ( &iobuf->list );
		free_iob ( iobuf );
	}
	free_iob ( tls->rx.handshake );
	privkey_put ( tls->client.key );
	x509_chain_put ( tls->client.chain );
	x509_chain_put ( tls->server.chain );
	x509_root_put ( tls->server.root );

	/* Drop reference to session */
	assert ( list_empty ( &tls->list ) );
	ref_put ( &session->refcnt );

	/* Free TLS structure itself */
	zfree ( tls );
}

/**
 * Finish with TLS connection
 *
 * @v tls		TLS connection
 * @v rc		Status code
 */
static void tls_close ( struct tls_connection *tls, int rc ) {

	/* Remove pending operations, if applicable */
	pending_put ( &tls->client.negotiation );
	pending_put ( &tls->server.negotiation );
	pending_put ( &tls->server.validation );

	/* Remove process */
	process_del ( &tls->tx.process );

	/* Close all interfaces */
	intf_shutdown ( &tls->cipherstream, rc );
	intf_shutdown ( &tls->plainstream, rc );
	intf_shutdown ( &tls->server.validator, rc );

	/* Remove from session */
	list_del ( &tls->list );
	INIT_LIST_HEAD ( &tls->list );

	/* Close secure channel */
	channel_close ( &tls->channel );

	/* Resume all other connections, in case we were the lead connection */
	tls_tx_resume_all ( tls->session );
}

/**
 * Send closure alert and finish with TLS connection
 *
 * @v tls		TLS connection
 * @v rc		Status code
 */
static void tls_close_alert ( struct tls_connection *tls, int rc ) {

	/* Send closure alert */
	tls_send_alert ( tls, TLS_ALERT_WARNING, TLS_ALERT_CLOSE_NOTIFY );

	/* Close connection */
	tls_close ( tls, rc );
}

/******************************************************************************
 *
 * Key schedule
 *
 ******************************************************************************
 */

/**
 * Generate deterministic connection nonce
 *
 * @v tls		TLS connection
 * @v random		Connection nonce to fill in
 *
 * The nonce is guaranteed to be deterministic and to be unique for
 * each connection (or renegotiation within a connection).
 *
 * We choose to regenerate it afresh whenever the value is required
 * (rather than generating it once and storing it) so that it is
 * impossible to accidentally use a stale nonce.
 */
static void tls_nonce ( struct tls_connection *tls,
			struct tls_random *nonce ) {
	static const char label[] = "tls connection nonce";

	/* Generate nonce as an ephemeral secret */
	channel_ephemeral_label ( &tls->channel, label, nonce,
				  sizeof ( *nonce ) );
}

/**
 * Clear key schedule digest algorithm
 *
 * @v tls		TLS connection
 */
static void tls_clear_digest ( struct tls_connection *tls ) {

	/* Inform secure channel that key material is being destroyed */
	channel_unkey ( &tls->channel );

	/* Stop key schedule */
	tlskey_stop ( &tls->key );
}

/**
 * Set key schedule digest algorithm
 *
 * @v tls		TLS connection
 * @v digest		Key schedule digest algorithm
 * @ret rc		Return status code
 */
static int tls_set_digest ( struct tls_connection *tls,
			    struct digest_algorithm *digest ) {
	const struct tls_key_schedule_operations *op;
	struct tls_random nonce;
	int rc;

	/* Clear existing key schedule digest algorithm */
	tls_clear_digest ( tls );

	/* Select key schedule */
	if ( tls_version ( tls, TLS_VERSION_TLS_1_2 ) ) {
		op = &tlskey_hash;
	} else {
		op = &tlskey_md5_sha1;
	}

	/* Generate client random bytes */
	tls_nonce ( tls, &nonce );

	/* Start key schedule */
	if ( ( rc = tlskey_start ( &tls->key, op, digest, &nonce ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not initialise key schedule: %s\n",
		       tls, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/******************************************************************************
 *
 * Cipher suite management
 *
 ******************************************************************************
 */

/** Null cipher suite */
struct tls_cipher_suite tls_cipher_suite_null = {
	.exchange = &tls_pubkey_exchange_algorithm,
	.pubkey = &pubkey_null,
	.cipher = &cipher_null,
	.digest = &digest_null,
	.handshake = &digest_null,
};

/** Number of supported cipher suites */
#define TLS_NUM_CIPHER_SUITES table_num_entries ( TLS_CIPHER_SUITES )

/**
 * Identify cipher suite
 *
 * @v cipher_suite	Cipher suite specification
 * @ret suite		Cipher suite, or NULL
 */
static struct tls_cipher_suite *
tls_find_cipher_suite ( unsigned int cipher_suite ) {
	struct tls_cipher_suite *suite;

	/* Identify cipher suite */
	for_each_table_entry ( suite, TLS_CIPHER_SUITES ) {
		if ( suite->code == cipher_suite )
			return suite;
	}

	return NULL;
}

/**
 * Clear cipher specification
 *
 * @v cipherspec	TLS cipher specification
 */
static void tls_clear_cipher ( struct tls_connection *tls __unused,
			       struct tls_cipherspec *cipherspec ) {

	zfree ( cipherspec->dynamic );
	memset ( cipherspec, 0, sizeof ( *cipherspec ) );
	cipherspec->suite = &tls_cipher_suite_null;
}

/**
 * Set cipher specification
 *
 * @v tls		TLS connection
 * @v cipherspec	TLS cipher specification
 * @v suite		Cipher suite
 * @ret rc		Return status code
 */
static int tls_set_cipher ( struct tls_connection *tls,
			    struct tls_cipherspec *cipherspec,
			    struct tls_cipher_suite *suite ) {
	size_t total;
	void *dynamic;

	/* Clear out old cipher contents, if any */
	tls_clear_cipher ( tls, cipherspec );

	/* Allocate dynamic storage */
	total = ( suite->key_len + suite->mac_len + suite->fixed_iv_len );
	dynamic = zalloc ( total );
	if ( ! dynamic ) {
		DBGC ( tls, "TLS %p could not allocate %zd bytes for crypto "
		       "context\n", tls, total );
		return -ENOMEM_CONTEXT;
	}

	/* Assign storage */
	cipherspec->dynamic = dynamic;
	cipherspec->cipher_key = dynamic;	dynamic += suite->key_len;
	cipherspec->mac_secret = dynamic;	dynamic += suite->mac_len;
	cipherspec->fixed_iv = dynamic;		dynamic += suite->fixed_iv_len;
	assert ( ( cipherspec->dynamic + total ) == dynamic );

	/* Store parameters */
	cipherspec->suite = suite;

	return 0;
}

/**
 * Select cipher suite
 *
 * @v tls		TLS connection
 * @v cipher_suite	Cipher suite specification
 * @ret rc		Return status code
 */
static int tls_select_cipher ( struct tls_connection *tls,
			       unsigned int cipher_suite ) {
	struct tls_cipher_suite *suite;
	struct digest_algorithm *digest;
	int rc;

	/* Identify cipher suite */
	suite = tls_find_cipher_suite ( cipher_suite );
	if ( ! suite ) {
		DBGC ( tls, "TLS %p does not support cipher %04x\n",
		       tls, ntohs ( cipher_suite ) );
		return -ENOTSUP_CIPHER;
	}

	/* Set (or clear) key exchange algorithm */
	tls->exchange = suite->exchange->exchange;

	/* Set key schedule digest algorithm */
	digest = ( tls_version ( tls, TLS_VERSION_TLS_1_2 ) ?
		   suite->handshake : &md5_sha1_algorithm );
	if ( ( rc = tls_set_digest ( tls, digest ) ) != 0 )
		return rc;

	/* Set ciphers */
	if ( ( rc = tls_set_cipher ( tls, &tls->tx.cipherspec.pending,
				     suite ) ) != 0 )
		return rc;
	if ( ( rc = tls_set_cipher ( tls, &tls->rx.cipherspec.pending,
				     suite ) ) != 0 )
		return rc;

	DBGC ( tls, "TLS %p selected cipher suite %s-%s-%s-%d-%s\n",
	       tls, suite->exchange->name, suite->pubkey->name,
	       suite->cipher->name, ( suite->key_len * 8 ),
	       suite->digest->name );

	return 0;
}

/**
 * Activate next cipher suite
 *
 * @v tls		TLS connection
 * @v pair		Cipher specification pair
 * @v pipe		Secure pipe
 * @ret rc		Return status code
 */
static int tls_change_cipher ( struct tls_connection *tls,
			       struct tls_cipherspec_pair *pair,
			       struct secure_pipe *pipe ) {
	struct tls_cipherspec *pending = &pair->pending;
	struct tls_cipherspec *active = &pair->active;
	struct tls_cipher_suite *suite = pending->suite;
	size_t mac_len = suite->mac_len;
	size_t key_len = suite->key_len;
	size_t iv_len = suite->fixed_iv_len;
	int rc;

	/* Sanity check */
	if ( suite == &tls_cipher_suite_null ) {
		DBGC ( tls, "TLS %p refusing to use null %s cipher\n",
		       tls, tls_pipe_name ( tls, pipe ) );
		return -ENOTSUP_NULL;
	}

	/* Generate traffic secret */
	if ( ( rc = tlskey_traffic ( &tls->key, pair->writer,
				     &tls_application ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not generate %s %s traffic secret: "
		       "%s\n", tls, tls_pipe_name ( tls, pipe ),
		       pair->writer->name, strerror ( rc ) );
		return rc;
	}

	/* Generate cipher key material */
	if ( ( rc = tlskey_cipher ( &tls->key, pair->writer,
				    pending->cipher_key, key_len,
				    pending->fixed_iv, iv_len,
				    pending->mac_secret, mac_len ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not generate %s %s keys: %s\n",
		       tls, tls_pipe_name ( tls, pipe ), pair->writer->name,
		       strerror ( rc ) );
		return rc;
	}

	/* Set cipher algorithm and key */
	if ( ( rc = channel_set_cipher ( &tls->channel, pipe, suite->cipher,
					 pending->cipher_key,
					 key_len ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not set %s cipher: %s\n",
		       tls, tls_pipe_name ( tls, pipe ), strerror ( rc ) );
		return rc;
	}

	/* Swap in new cipher suite */
	tls_clear_cipher ( tls, active );
	memswap ( active, pending, sizeof ( *active ) );
	DBGC ( tls, "TLS %p activated %s cipher %s-%s-%s-%d-%s\n",
	       tls, tls_pipe_name ( tls, pipe ), suite->exchange->name,
	       suite->pubkey->name, suite->cipher->name,
	       ( suite->key_len * 8 ), suite->digest->name );

	return 0;
}

/******************************************************************************
 *
 * Signature and hash algorithms
 *
 ******************************************************************************
 */

/** Number of supported signature and hash algorithms */
#define TLS_NUM_SIG_HASH_ALGORITHMS \
	table_num_entries ( TLS_SIG_HASH_ALGORITHMS )

/**
 * Find TLS signature and hash algorithm
 *
 * @v pubkey		Public-key algorithm
 * @v digest		Digest algorithm
 * @ret sig_hash	Signature and hash algorithm, or NULL
 */
static struct tls_signature_hash_algorithm *
tls_signature_hash_algorithm ( struct pubkey_algorithm *pubkey,
			       struct digest_algorithm *digest ) {
	struct tls_signature_hash_algorithm *sig_hash;

	/* Identify signature and hash algorithm */
	for_each_table_entry ( sig_hash, TLS_SIG_HASH_ALGORITHMS ) {
		if ( ( sig_hash->pubkey == pubkey ) &&
		     ( sig_hash->digest == digest ) ) {
			return sig_hash;
		}
	}

	return NULL;
}

/**
 * Find TLS signature and hash algorithm
 *
 * @v code		Signature and hash algorithm identifier
 * @ret sig_hash	Signature and hash algorithm, or NULL
 */
static struct tls_signature_hash_algorithm *
tls_find_signature_hash ( unsigned int code ) {
	struct tls_signature_hash_algorithm *sig_hash;

	/* Identify signature and hash algorithm */
	for_each_table_entry ( sig_hash, TLS_SIG_HASH_ALGORITHMS ) {
		if ( sig_hash->code == code )
			return sig_hash;
	}

	return NULL;
}

/******************************************************************************
 *
 * Named key exchange groups
 *
 ******************************************************************************
 */

/**
 * Identify named key exchange group
 *
 * @v named_group	Named group specification
 * @ret group		Named group, or NULL
 */
static struct tls_named_group *
tls_find_named_group ( unsigned int named_group ) {
	struct tls_named_group *group;

	/* Identify named group */
	for_each_table_entry ( group, TLS_NAMED_GROUPS ) {
		if ( group->code && ( group->code == named_group ) )
			return group;
	}

	return NULL;
}

/**
 * Identify named key exchange group by Diffie-Hellman parameters
 *
 * @v dh_p		Prime modulus
 * @v dh_p_len		Length of prime modulus
 * @v dh_g		Generator
 * @v dh_g_len		Length of generator
 * @ret group		Named group, or NULL
 */
static struct tls_named_group *
tls_find_param_group ( const void *dh_p, size_t dh_p_len, const void *dh_g,
		       size_t dh_g_len ) {
	struct tls_named_group *group;

	/* Identify named group by parameters */
	for_each_table_entry ( group, TLS_NAMED_GROUPS ) {
		if ( is_ffdhe ( group->exchange ) &&
		     ffdhe_has_params ( group->exchange, dh_p, dh_p_len,
					dh_g, dh_g_len ) ) {
			return group;
		}
	}

	return NULL;
}

/** Public key exchange algorithm */
struct tls_key_exchange_algorithm tls_pubkey_exchange_algorithm = {
	.name = "pubkey",
	.exchange = &tls_classic_pre_master_algorithm,
	.len_len = sizeof ( uint16_t ),
};

/**
 * Parse key exchange parameters from DHE Server Key Exchange record
 *
 * @v tls		TLS connection
 * @v data		Server Key Exchange handshake record
 * @v len		Length of Server Key Exchange handshake record
 * @v params		Key exchange parameters to fill in
 * @ret rc		Return status code
 */
static int tls_parse_dhe ( struct tls_connection *tls,
			   const void *data, size_t len,
			   struct tls_key_exchange_parameters *params ) {
	struct tls_named_group *group;
	const struct {
		uint16_t len;
		uint8_t data[0];
	} __attribute__ (( packed )) *dh_val[3];
	typeof ( dh_val[0] ) dh_p;
	typeof ( dh_val[1] ) dh_g;
	typeof ( dh_val[2] ) dh_ys;
	const void *param;
	size_t remaining;
	size_t frag_len;
	unsigned int i;

	/* Parse ServerKeyExchange */
	param = data;
	remaining = len;
	for ( i = 0 ; i < ( sizeof ( dh_val ) / sizeof ( dh_val[0] ) ) ; i++ ){
		dh_val[i] = param;
		if ( ( sizeof ( *dh_val[i] ) > remaining ) ||
		     ( ntohs ( dh_val[i]->len ) > ( remaining -
						    sizeof ( *dh_val[i] ) ) )){
			DBGC ( tls, "TLS %p received underlength "
			       "ServerKeyExchange\n", tls );
			DBGC_HDA ( tls, 0, data, len );
			return -EINVAL_KEY_EXCHANGE;
		}
		frag_len = ( sizeof ( *dh_val[i] ) + ntohs ( dh_val[i]->len ));
		param += frag_len;
		remaining -= frag_len;
	}
	params->len = ( len - remaining );

	/* Identify named group and partner key */
	dh_p = dh_val[0];
	dh_g = dh_val[1];
	dh_ys = dh_val[2];
	group = tls_find_param_group ( dh_p->data, ntohs ( dh_p->len ),
				       dh_g->data, ntohs ( dh_g->len ) );
	if ( ! group ) {
		DBGC ( tls, "TLS %p unsupported %d-bit group:\n",
		       tls, ( 8 * ntohs ( dh_p->len ) ) );
		DBGC_HDA ( tls, 0, data, len );
		return -ENOTSUP_GROUP;
	}
	params->exchange = group->exchange;
	params->partner = dh_ys->data;
	params->partner_len = ntohs ( dh_ys->len );

	return 0;
}

/** Ephemeral Diffie-Hellman key exchange algorithm */
struct tls_key_exchange_algorithm tls_dhe_exchange_algorithm = {
	.name = "dhe",
	.parse = tls_parse_dhe,
	.len_len = sizeof ( uint16_t ),
};

/**
 * Parse key exchange parameters from ECDHE Server Key Exchange record
 *
 * @v tls		TLS connection
 * @v data		Server Key Exchange handshake record
 * @v len		Length of Server Key Exchange handshake record
 * @v params		Key exchange parameters to fill in
 * @ret rc		Return status code
 */
static int tls_parse_ecdhe ( struct tls_connection *tls,
			     const void *data, size_t len,
			     struct tls_key_exchange_parameters *params ) {
	struct tls_named_group *group;
	const struct {
		uint8_t curve_type;
		uint16_t named_group;
		uint8_t public_len;
		uint8_t public[0];
	} __attribute__ (( packed )) *ecdh = data;

	/* Parse ServerKeyExchange record */
	if ( ( sizeof ( *ecdh ) > len ) ||
	     ( ecdh->public_len > ( len - sizeof ( *ecdh ) ) ) ) {
		DBGC ( tls, "TLS %p received underlength ServerKeyExchange\n",
		       tls );
		DBGC_HDA ( tls, 0, data, len );
		return -EINVAL_KEY_EXCHANGE;
	}
	params->len = ( sizeof ( *ecdh ) + ecdh->public_len );

	/* Identify named group and partner key */
	if ( ecdh->curve_type != TLS_NAMED_CURVE_TYPE ) {
		DBGC ( tls, "TLS %p unsupported curve type %d\n",
		       tls, ecdh->curve_type );
		DBGC_HDA ( tls, 0, data, len );
		return -ENOTSUP_GROUP;
	}
	group = tls_find_named_group ( ecdh->named_group );
	if ( ! group ) {
		DBGC ( tls, "TLS %p unsupported named group %d\n",
		       tls, ntohs ( ecdh->named_group ) );
		DBGC_HDA ( tls, 0, data, len );
		return -ENOTSUP_GROUP;
	}
	params->exchange = group->exchange;
	params->partner = ecdh->public;
	params->partner_len = ecdh->public_len;

	return 0;
}

/** Ephemeral Elliptic Curve Diffie-Hellman key exchange algorithm */
struct tls_key_exchange_algorithm tls_ecdhe_exchange_algorithm = {
	.name = "ecdhe",
	.parse = tls_parse_ecdhe,
	.len_len = sizeof ( uint8_t ),
};

/**
 * Check if key exchange keys have a variable size
 *
 * @v tls		TLS connection
 * @v exchange		Key exchange algorithm
 * @ret is_variable	Key exchange keys have a variable size
 *
 * TLS versions 1.2 and earlier treat FFDHE public and shared keys as
 * unsigned big-endian integers using a minimal byte representation.
 * For all other purposes, key exchange keys have a fixed size
 * determined by the key exchange algorithm.
 */
static int tls_keysize_is_variable ( struct tls_connection *tls,
				     struct exchange_algorithm *exchange ) {

	/* TLS versions 1.3 and later always have fixed-size keys */
	if ( tls_version ( tls, TLS_VERSION_TLS_1_3 ) )
		return 0;

	/* TLS versions 1.2 and earlier have variable-sized FFDHE keys */
	return is_ffdhe ( exchange );
}

/******************************************************************************
 *
 * Key agreement
 *
 ******************************************************************************
 */

/**
 * Share public key
 *
 * @v tls		TLS connection
 * @v exchange		Key exchange algorithm
 * @v public		Public key to fill in
 * @v len		Length of public key
 * @ret rc		Return status code
 */
static int tls_key_share ( struct tls_connection *tls,
			   struct exchange_algorithm *exchange,
			   void *public, size_t len ) {
	struct secure_channel *channel = &tls->channel;
	size_t pubsize = exchange->pubsize;
	int rc;

	/* Check key length */
	if ( pubsize != len ) {
		DBGC ( tls, "TLS %p wrong public %s key size (%zd bytes)\n",
		       tls, exchange->name, len );
		return -EINVAL_KEY_EXCHANGE;
	}

	/* Share public key */
	if ( ( rc = channel_key_share ( channel, exchange, public ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not share public %s key: %s\n",
		       tls, exchange->name, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Agree shared secret
 *
 * @v tls		TLS connection
 * @v exchange		Key exchange algorithm
 * @v partner		Partner public key
 * @v len		Length of partner public key
 * @ret rc		Return status code
 */
static int tls_key_agree ( struct tls_connection *tls,
			   struct exchange_algorithm *exchange,
			   const void *partner, size_t len ) {
	struct secure_channel *channel = &tls->channel;
	size_t pubsize = exchange->pubsize;
	uint8_t *tmp;
	size_t pad_len;
	int strip;
	int rc;

	/* Allocate space for potentially padded partner key */
	tmp = zalloc ( pubsize );
	if ( ! tmp ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Validate partner key */
	if ( len > pubsize ) {
		DBGC ( tls, "TLS %p overlength partner %s key:\n",
		       tls, exchange->name );
		DBGC_HDA ( tls, 0, partner, len );
		rc = -EINVAL_KEY_EXCHANGE;
		goto err_len;
	}

	/* TLSv1.2 and earlier may require zero-padding for FFDHE keys */
	strip = tls_keysize_is_variable ( tls, exchange );
	pad_len = ( pubsize - len );
	if ( pad_len && ( ! strip ) ) {
		DBGC ( tls, "TLS %p underlength partner %s key:\n",
		       tls, exchange->name );
		DBGC_HDA ( tls, 0, partner, len );
		rc = -EINVAL_KEY_EXCHANGE;
		goto err_pad;
	}
	memcpy ( ( tmp + pad_len ), partner, len );

	/* Agree shared secret */
	if ( ( rc = channel_key_agree ( channel, exchange, tmp ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not agree shared %s key: %s\n",
		       tls, exchange->name, strerror ( rc ) );
		goto err_agree;
	}

 err_agree:
 err_pad:
 err_len:
	zfree ( tmp );
 err_alloc:
	return rc;
}

/**
 * Encrypt (and implicitly bind) shared secret
 *
 * @v tls		TLS connection
 * @v exchange		Key exchange algorithm
 * @v builder		ASN.1 builder
 * @ret rc		Return status code
 */
static int tls_key_encrypt ( struct tls_connection *tls,
			     struct exchange_algorithm *exchange,
			     struct asn1_builder *builder ) {
	struct secure_channel *channel = &tls->channel;
	struct x509_certificate *cert;
	struct pubkey_algorithm *pubkey;
	int rc;

	/* Identify server certificate */
	cert = x509_first ( tls->server.chain );
	if ( ! cert ) {
		DBGC ( tls, "TLS %p has no server certificate\n", tls );
		return -ENOENT_CERT;
	}
	pubkey = cert->subject.public_key.algorithm->pubkey;

	/* Encrypt (and implicitly bind) shared secret */
	if ( ( rc = channel_bind_encrypt ( channel, cert, exchange, pubkey,
					   builder ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not encrypt %s key: %s\n",
		       tls, exchange->name, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Build shareable key
 *
 * @v tls		TLS connection
 * @v exchange		Key exchange algorithm
 * @v builder		ASN.1 builder
 * @ret rc		Return status code
 */
static int tls_key_build ( struct tls_connection *tls,
			   struct exchange_algorithm *exchange,
			   struct asn1_builder *builder ) {
	size_t pubsize = exchange->pubsize;
	int rc;

	/* Share or encrypt as applicable */
	if ( pubsize ) {

		/* Allocate space for public key */
		if ( ( rc = asn1_grow ( builder, pubsize ) ) != 0 )
			return rc;

		/* Share public key */
		if ( ( rc = tls_key_share ( tls, exchange, builder->data,
					    pubsize ) ) != 0 ) {
			return rc;
		}

	} else {

		/* Encrypt (and implicitly bind) shared secret */
		if ( ( rc = tls_key_encrypt ( tls, exchange,
					      builder ) ) != 0 ) {
			return rc;
		}
	}

	return 0;
}

/******************************************************************************
 *
 * Secure channel operations
 *
 ******************************************************************************
 */

/**
 * Reset the key schedule
 *
 * @v channel		Secure channel
 */
static void tls_channel_reset ( struct secure_channel *channel ) {
	struct tls_connection *tls =
		container_of ( channel, struct tls_connection, channel );

	/* Reset key schedule */
	tlskey_reset ( &tls->key );
}

/**
 * Apply a new shared secret to key schedule
 *
 * @v channel		Secure channel
 * @v exchange		Key exchange algorithm
 * @v shared		New shared secret
 * @v accumulated	Accumulation flag to fill in
 * @ret rc		Return status code
 */
static int tls_channel_apply ( struct secure_channel *channel,
			       struct exchange_algorithm *exchange,
			       const void *shared, int *accumulated ) {
	struct tls_connection *tls =
		container_of ( channel, struct tls_connection, channel );
	size_t shared_len = exchange->sharedsize;
	int rc;

	/* Strip leading zeros if needed */
	if ( tls_keysize_is_variable ( tls, exchange ) ) {
		/* TLS v1.2 and earlier strip leading zeros for FFDHE
		 *
		 * This code can be reached only with the result from
		 * a successful FFDHE key exchange, and so the shared
		 * secret cannot ever end up as all zeros.
		 */
		while ( shared_len && ( ! *( ( const uint8_t * ) shared ) ) ) {
			shared++;
			shared_len--;
		}
		assert ( shared_len > 0 );
	}
	DBGC ( tls, "TLS %p shared (pre-master) secret:\n", tls );
	DBGC_HDA ( tls, 0, shared, shared_len );

	/* Apply shared secret to key schedule */
	if ( ( rc = tlskey_apply ( &tls->key, shared, shared_len ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not apply shared secret: %s\n",
		       tls, strerror ( rc ) );
		return rc;
	}

	/* Set accumulation flag if applicable */
	*accumulated = tlskey_is_accumulating ( &tls->key );

	return 0;
}

/**
 * Save a pre-shared key for future resumption of the key schedule
 *
 * @v channel		Secure channel
 * @v psid		Pre-shared bound peer identity
 * @ret rc		Return status code
 */
static int tls_channel_save ( struct secure_channel *channel,
			      struct secure_preshared_identity *psid ) {
	struct tls_connection *tls =
		container_of ( channel, struct tls_connection, channel );
	struct tls_session *session =
		container_of ( psid, struct tls_session, psid );
	int rc;

	/* We support saving pre-shared keys only once the secure
	 * channel has been established (since resumed connections
	 * will not receive a certificate chain and so will have no
	 * further opportunities to validate the bound identity).
	 */
	if ( ! channel_is_established ( channel ) ) {
		DBGC ( tls, "TLS %p cannot save pre-shared key before "
		       "channel is established\n", tls );
		return -EPERM_SAVE;
	}

	/* Save key material */
	if ( ( rc = tlskey_save ( &tls->key, NULL, 0,
				  &session->psk ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not save key material: %s\n",
		       tls, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Load a pre-shared key and resume the key schedule
 *
 * @v channel		Secure channel
 * @v psid		Pre-shared bound peer identity
 * @ret rc		Return status code
 */
static int tls_channel_load ( struct secure_channel *channel,
			      struct secure_preshared_identity *psid ) {
	struct tls_connection *tls =
		container_of ( channel, struct tls_connection, channel );
	struct tls_session *session =
		container_of ( psid, struct tls_session, psid );
	int rc;

	/* Load key material */
	if ( ( rc = tlskey_load ( &tls->key, tls->extended_master_secret,
				  &session->psk ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not load key material: %s\n",
		       tls, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Verify authenticator value
 *
 * @v channel		Secure channel
 * @v auth		Authenticator value
 * @v len		Length of authenticator value
 * @ret rc		Return status code
 */
static int tls_channel_verify ( struct secure_channel *channel,
				const void *auth, size_t len ) {
	struct tls_connection *tls =
		container_of ( channel, struct tls_connection, channel );
	int rc;

	/* Sanity checks */
	if ( len != sizeof ( tls->verify.server ) ) {
		DBGC ( tls, "TLS %p invalid authenticator value:\n", tls );
		DBGC_HDA ( tls, 0, auth, len );
		return -EPERM_VERIFY;
	}

	/* Generate verification data */
	if ( ( rc = tlskey_verify ( &tls->key, &tls_server,
				    tls->verify.server,
				    sizeof ( tls->verify.server ) ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not generate server verification: "
		       "%s\n", tls, strerror ( rc ) );
		return rc;
	}

	/* Verify data */
	if ( memcmp ( tls->verify.server, auth, len ) != 0 ) {
		DBGC ( tls, "TLS %p incorrect authenticator value:\n", tls );
		return -EPERM_VERIFY;
	}

	return 0;
}

/** Secure channel operations */
static struct secure_channel_operations tls_channel_ops = {
	.reset = tls_channel_reset,
	.apply = tls_channel_apply,
	.save = tls_channel_save,
	.load = tls_channel_load,
	.verify = tls_channel_verify,
};

/******************************************************************************
 *
 * Session management
 *
 ******************************************************************************
 */

/**
 * Find or create session for TLS connection
 *
 * @v tls		TLS connection
 * @v name		Server name
 * @ret rc		Return status code
 */
static int tls_session ( struct tls_connection *tls, const char *name ) {
	static const char label[] = "tls session id";
	struct tls_session *session;
	char *name_copy;
	int rc;

	/* Find existing matching session, if any */
	list_for_each_entry ( session, &tls_sessions, list ) {
		if ( ( strcmp ( name, session->name ) == 0 ) &&
		     ( tls->server.root == session->root ) &&
		     ( tls->client.key == session->key ) ) {
			ref_get ( &session->refcnt );
			tls->session = session;
			DBGC ( tls, "TLS %p joining session %s\n", tls, name );
			return 0;
		}
	}

	/* Create new session */
	session = zalloc ( sizeof ( *session ) + strlen ( name )
			   + 1 /* NUL */ );
	if ( ! session ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	ref_init ( &session->refcnt, free_tls_session );
	name_copy = ( ( ( void * ) session ) + sizeof ( *session ) );
	strcpy ( name_copy, name );
	session->name = name_copy;
	session->root = x509_root_get ( tls->server.root );
	session->key = privkey_get ( tls->client.key );
	INIT_LIST_HEAD ( &session->conn );
	list_add ( &session->list, &tls_sessions );

	/* Generate random initial session ID */
	channel_ephemeral_label ( &tls->channel, label, session->id.data,
				  sizeof ( session->id.data ) );
	session->id.len = sizeof ( session->id.data );

	/* Record session */
	tls->session = session;

	DBGC ( tls, "TLS %p created session %s\n", tls, name );
	return 0;

	ref_put ( &session->refcnt );
 err_alloc:
	return rc;
}

/**
 * Save session for future resumption
 *
 * @v tls		TLS connection
 * @ret rc		Return status code
 */
static int tls_save ( struct tls_connection *tls ) {
	struct tls_session *session = tls->session;
	int rc;

	/* Save pre-shared key and peer identity */
	if ( ( rc = channel_save ( &tls->channel, &session->psid ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not save: %s\n",
		       tls, strerror ( rc ) );
		return rc;
	}

	/* Record new session ID, if provided */
	if ( tls->new_id.len ) {
		memcpy ( &session->id, &tls->new_id, sizeof ( session->id ) );
		DBGC ( tls, "TLS %p saved session ID:\n", tls );
		DBGC_HDA ( tls, 0, session->id.data, session->id.len );
	}

	/* Record (and consume) new session ticket, if provided */
	if ( tls->new_ticket.len ) {
		zfree ( session->ticket.data );
		session->ticket.data = tls->new_ticket.data;
		session->ticket.len = tls->new_ticket.len;
		tls->new_ticket.data = NULL;
		tls->new_ticket.len = 0;
	}

	return 0;
}

/**
 * Resume session
 *
 * @v tls		TLS connection
 * @ret rc		Return status code
 */
static int tls_resume ( struct tls_connection *tls ) {
	struct tls_session *session = tls->session;
	int rc;

	DBGC ( tls, "TLS %p resuming session ID:\n", tls );
	DBGC_HDA ( tls, 0, session->id.data, session->id.len );

	/* Load pre-shared key and peer identity */
	if ( ( rc = channel_load ( &tls->channel, &session->psid ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not resume: %s\n",
		       tls, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/******************************************************************************
 *
 * Record handling
 *
 ******************************************************************************
 */

/**
 * Add handshake record to verification hash
 *
 * @v tls		TLS connection
 * @v data		Handshake record
 * @v len		Length of handshake record
 * @ret rc		Return status code
 */
static int tls_add_handshake ( struct tls_connection *tls,
			       const void *data, size_t len ) {

	/* Record in transcript digest */
	tlskey_digest ( &tls->key, data, len );

	return 0;
}

/**
 * Resume TX state machine
 *
 * @v tls		TLS connection
 */
static void tls_tx_resume ( struct tls_connection *tls ) {
	process_add ( &tls->tx.process );
}

/**
 * Resume TX state machine for all connections within a session
 *
 * @v session		TLS session
 */
static void tls_tx_resume_all ( struct tls_session *session ) {
	struct tls_connection *tls;

	list_for_each_entry ( tls, &session->conn, list )
		tls_tx_resume ( tls );
}

/**
 * Restart negotiation
 *
 * @v tls		TLS connection
 */
static void tls_restart ( struct tls_connection *tls ) {

	/* Sanity check */
	assert ( ! tls->tx.pending );
	assert ( ! is_pending ( &tls->client.negotiation ) );
	assert ( ! is_pending ( &tls->server.negotiation ) );
	assert ( ! is_pending ( &tls->server.validation ) );

	/* Reset secure channel */
	channel_reopen ( &tls->channel );

	/* (Re)start negotiation */
	tls->tx.pending = TLS_TX_CLIENT_HELLO;
	tls_tx_resume ( tls );
	pending_get ( &tls->client.negotiation );
	pending_get ( &tls->server.negotiation );
}

/**
 * Establish secure channel
 *
 * @v tls		TLS connection
 * @ret rc		Return status code
 */
static int tls_establish ( struct tls_connection *tls ) {
	struct tls_session *session = tls->session;
	int rc;

	/* Establish channel as trusted for server name */
	if ( ( rc = channel_establish ( &tls->channel, session->name,
					tls->server.root ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not establish channel: %s\n",
		       tls, strerror ( rc ) );
		return rc;
	}

	/* Save session for future resumption, if applicable */
	if ( tls->new_id.len || tls->new_ticket.len )
		tls_save ( tls );

	/* Move to end of session's connection list and allow other
	 * connections to start making progress.
	 */
	list_del ( &tls->list );
	list_add_tail ( &tls->list, &session->conn );
	tls_tx_resume_all ( session );

	/* Send notification of a window change */
	xfer_window_changed ( &tls->plainstream );

	return 0;
}

/**
 * Transmit Handshake record
 *
 * @v tls		TLS connection
 * @v data		Plaintext record
 * @v len		Length of plaintext record
 * @ret rc		Return status code
 */
static int tls_send_handshake ( struct tls_connection *tls,
				const void *data, size_t len ) {

	/* Send record */
	return tls_send_plaintext ( tls, TLS_TYPE_HANDSHAKE, data, len );
}

/**
 * Digest or transmit Client Hello record
 *
 * @v tls		TLS connection
 * @v action		Action to take on Client Hello record
 * @ret rc		Return status code
 */
static int tls_client_hello ( struct tls_connection *tls,
			      int ( * action ) ( struct tls_connection *tls,
						 const void *data,
						 size_t len ) ) {
	struct tls_session *session = tls->session;
	size_t name_len = strlen ( session->name );
	struct {
		uint16_t type;
		uint16_t len;
		struct {
			uint16_t len;
			struct {
				uint8_t type;
				uint16_t len;
				uint8_t name[name_len];
			} __attribute__ (( packed )) list[1];
		} __attribute__ (( packed )) data;
	} __attribute__ (( packed )) *server_name_ext;
	struct {
		uint16_t type;
		uint16_t len;
		struct {
			uint8_t max;
		} __attribute__ (( packed )) data;
	} __attribute__ (( packed )) *max_fragment_length_ext;
	struct {
		uint16_t type;
		uint16_t len;
		struct {
			uint16_t len;
			uint16_t code[TLS_NUM_SIG_HASH_ALGORITHMS];
		} __attribute__ (( packed )) data;
	} __attribute__ (( packed )) *signature_algorithms_ext;
	struct {
		uint16_t type;
		uint16_t len;
		struct {
			uint8_t len;
			uint8_t data[ tls->secure_renegotiation ?
				      sizeof ( tls->verify.client ) :0 ];
		} __attribute__ (( packed )) data;
	} __attribute__ (( packed )) *renegotiation_info_ext;
	struct {
		uint16_t type;
		uint16_t len;
		struct {
			uint8_t data[session->ticket.len];
		} __attribute__ (( packed )) data;
	} __attribute__ (( packed )) *session_ticket_ext;
	struct {
		uint16_t type;
		uint16_t len;
		struct {
			uint16_t len;
			uint16_t code[TLS_NUM_NAMED_GROUPS];
		} __attribute__ (( packed )) data;
	} __attribute__ (( packed )) *named_group_ext;
	struct {
		uint16_t type;
		uint16_t len;
	} __attribute__ (( packed )) *extended_master_secret_ext;
	struct {
		typeof ( *server_name_ext ) server_name;
		typeof ( *max_fragment_length_ext ) max_fragment_length;
		typeof ( *signature_algorithms_ext ) signature_algorithms;
		typeof ( *renegotiation_info_ext ) renegotiation_info;
		typeof ( *session_ticket_ext ) session_ticket;
		typeof ( *extended_master_secret_ext ) extended_master_secret;
		typeof ( *named_group_ext )
			named_group[TLS_NUM_NAMED_GROUPS ? 1 : 0];
	} __attribute__ (( packed )) *extensions;
	struct {
		uint32_t type_length;
		uint16_t version;
		struct tls_random random;
		uint8_t session_id_len;
		uint8_t session_id[session->id.len];
		uint16_t cipher_suite_len;
		uint16_t cipher_suites[TLS_NUM_CIPHER_SUITES];
		uint8_t compression_methods_len;
		uint8_t compression_methods[1];
		uint16_t extensions_len;
		typeof ( *extensions ) extensions;
	} __attribute__ (( packed )) hello;
	struct tls_cipher_suite *suite;
	struct tls_signature_hash_algorithm *sighash;
	struct tls_named_group *group;
	unsigned int i;

	/* Construct record */
	memset ( &hello, 0, sizeof ( hello ) );
	hello.type_length = ( cpu_to_le32 ( TLS_CLIENT_HELLO ) |
			      htonl ( sizeof ( hello ) -
				      sizeof ( hello.type_length ) ) );
	hello.version = htons ( TLS_VERSION_MAX );
	tls_nonce ( tls, &hello.random );
	hello.session_id_len = session->id.len;
	memcpy ( hello.session_id, session->id.data,
		 sizeof ( hello.session_id ) );
	hello.cipher_suite_len = htons ( sizeof ( hello.cipher_suites ) );
	i = 0 ; for_each_table_entry ( suite, TLS_CIPHER_SUITES )
		hello.cipher_suites[i++] = suite->code;
	hello.compression_methods_len = sizeof ( hello.compression_methods );
	hello.extensions_len = htons ( sizeof ( hello.extensions ) );
	extensions = &hello.extensions;

	/* Construct server name extension */
	server_name_ext = &extensions->server_name;
	server_name_ext->type = htons ( TLS_SERVER_NAME );
	server_name_ext->len = htons ( sizeof ( server_name_ext->data ) );
	server_name_ext->data.len
		= htons ( sizeof ( server_name_ext->data.list ) );
	server_name_ext->data.list[0].type = TLS_SERVER_NAME_HOST_NAME;
	server_name_ext->data.list[0].len
		= htons ( sizeof ( server_name_ext->data.list[0].name ) );
	memcpy ( server_name_ext->data.list[0].name, session->name,
		 sizeof ( server_name_ext->data.list[0].name ) );

	/* Construct maximum fragment length extension */
	max_fragment_length_ext = &extensions->max_fragment_length;
	max_fragment_length_ext->type = htons ( TLS_MAX_FRAGMENT_LENGTH );
	max_fragment_length_ext->len
		= htons ( sizeof ( max_fragment_length_ext->data ) );
	max_fragment_length_ext->data.max = TLS_MAX_FRAGMENT_LENGTH_VALUE;

	/* Construct supported signature algorithms extension */
	signature_algorithms_ext = &extensions->signature_algorithms;
	signature_algorithms_ext->type = htons ( TLS_SIGNATURE_ALGORITHMS );
	signature_algorithms_ext->len
		= htons ( sizeof ( signature_algorithms_ext->data ) );
	signature_algorithms_ext->data.len
		= htons ( sizeof ( signature_algorithms_ext->data.code ) );
	i = 0 ; for_each_table_entry ( sighash, TLS_SIG_HASH_ALGORITHMS )
		signature_algorithms_ext->data.code[i++] = sighash->code;

	/* Construct renegotiation information extension */
	renegotiation_info_ext = &extensions->renegotiation_info;
	renegotiation_info_ext->type = htons ( TLS_RENEGOTIATION_INFO );
	renegotiation_info_ext->len
		= htons ( sizeof ( renegotiation_info_ext->data ) );
	renegotiation_info_ext->data.len
		= sizeof ( renegotiation_info_ext->data.data );
	memcpy ( renegotiation_info_ext->data.data, tls->verify.client,
		 sizeof ( renegotiation_info_ext->data.data ) );

	/* Construct session ticket extension */
	session_ticket_ext = &extensions->session_ticket;
	session_ticket_ext->type = htons ( TLS_SESSION_TICKET );
	session_ticket_ext->len
		= htons ( sizeof ( session_ticket_ext->data ) );
	memcpy ( session_ticket_ext->data.data, session->ticket.data,
		 sizeof ( session_ticket_ext->data.data ) );

	/* Construct extended master secret extension */
	extended_master_secret_ext = &extensions->extended_master_secret;
	extended_master_secret_ext->type
		= htons ( TLS_EXTENDED_MASTER_SECRET );
	extended_master_secret_ext->len = 0;

	/* Construct named groups extension, if applicable */
	if ( sizeof ( extensions->named_group ) ) {
		named_group_ext = &extensions->named_group[0];
		named_group_ext->type = htons ( TLS_NAMED_GROUP );
		named_group_ext->len
			= htons ( sizeof ( named_group_ext->data ) );
		named_group_ext->data.len
			= htons ( sizeof ( named_group_ext->data.code ) );
		i = 0 ; for_each_table_entry ( group, TLS_NAMED_GROUPS ) {
			if ( group->code )
				named_group_ext->data.code[i++] = group->code;
		}
		assert ( i == TLS_NUM_NAMED_GROUPS );
	}

	return action ( tls, &hello, sizeof ( hello ) );
}

/**
 * Transmit Client Hello record
 *
 * @v tls		TLS connection
 * @ret rc		Return status code
 */
static int tls_send_client_hello ( struct tls_connection *tls ) {

	return tls_client_hello ( tls, tls_send_handshake );
}

/**
 * Transmit Certificate record
 *
 * @v tls		TLS connection
 * @ret rc		Return status code
 */
static int tls_send_certificate ( struct tls_connection *tls ) {
	struct {
		tls24_t length;
		uint8_t data[0];
	} __attribute__ (( packed )) *certificate;
	struct {
		uint32_t type_length;
		tls24_t length;
		typeof ( *certificate ) certificates[0];
	} __attribute__ (( packed )) *certificates;
	struct x509_link *link;
	struct x509_certificate *cert;
	struct io_buffer *iobuf;
	size_t len;

	/* Calculate length of client certificates */
	len = 0;
	list_for_each_entry ( link, &tls->client.chain->links, list ) {
		cert = link->cert;
		len += ( sizeof ( *certificate ) + cert->raw.len );
		DBGC ( tls, "TLS %p sending client certificate %s\n",
		       tls, x509_name ( cert ) );
	}

	/* Allocate storage for Certificate record (which may be too
	 * large for the stack).
	 */
	iobuf = tls_alloc_iob ( tls, ( sizeof ( *certificates ) + len ) );
	if ( ! iobuf )
		return -ENOMEM_CERTIFICATE;

	/* Populate record */
	certificates = iob_put ( iobuf, sizeof ( *certificates ) );
	certificates->type_length =
		( cpu_to_le32 ( TLS_CERTIFICATE ) |
		  htonl ( sizeof ( *certificates ) + len -
			  sizeof ( certificates->type_length ) ) );
	tls_set_uint24 ( &certificates->length, len );
	list_for_each_entry ( link, &tls->client.chain->links, list ) {
		cert = link->cert;
		certificate = iob_put ( iobuf, sizeof ( *certificate ) );
		tls_set_uint24 ( &certificate->length, cert->raw.len );
		memcpy ( iob_put ( iobuf, cert->raw.len ), cert->raw.data,
			 cert->raw.len );
	}

	/* Transmit record */
	return tls_send_record ( tls, TLS_TYPE_HANDSHAKE,
				 iob_disown ( iobuf ) );
}

/**
 * Transmit Client Key Exchange record
 *
 * @v tls		TLS connection
 * @ret rc		Return status code
 */
static int tls_send_client_key_exchange ( struct tls_connection *tls ) {
	struct tls_cipherspec *cipherspec = &tls->tx.cipherspec.pending;
	struct tls_cipher_suite *suite = cipherspec->suite;
	struct exchange_algorithm *exchange = tls->exchange;
	struct {
		uint32_t type_length;
		uint8_t key_len[suite->exchange->len_len];
		uint8_t key[0];
	} __attribute__ (( packed )) key_xchg;
	struct asn1_builder builder = { NULL, 0 };
	size_t len;
	int i;
	int rc;

	/* Fail if we have not selected a key exchange algorithm */
	if ( ! exchange ) {
		DBGC ( tls, "TLS %p has no key exchange algorithm\n", tls );
		rc = -ENOENT_KEY_EXCHANGE;
		goto err_exchange;
	}

	/* Build shareable key */
	if ( ( rc = tls_key_build ( tls, exchange, &builder ) ) != 0 )
		goto err_build;
	len = builder.len;

	/* Construct record header */
	key_xchg.type_length =
		( cpu_to_le32 ( TLS_CLIENT_KEY_EXCHANGE ) |
		  htonl ( len + sizeof ( key_xchg ) -
			  sizeof ( key_xchg.type_length ) ) );
	for ( i = ( sizeof ( key_xchg.key_len ) - 1 ) ; i >= 0 ; i-- ) {
		key_xchg.key_len[i] = ( len & 0xff );
		len >>= 8;
	}
	assert ( len == 0 );

	/* Prepend record header (as raw data in ASN.1 builder) */
	if ( ( rc = asn1_prepend_raw ( &builder, &key_xchg,
				       sizeof ( key_xchg ) ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not construct Client Key "
		       "Exchange: %s\n", tls, strerror ( rc ) );
		goto err_prepend;
	}

	/* Transmit Client Key Exchange record */
	if ( ( rc = tls_send_handshake ( tls, builder.data,
					 builder.len ) ) != 0 ) {
		goto err_send;
	}

	/* Generate master secret */
	if ( ( rc = tlskey_master ( &tls->key,
				    tls->extended_master_secret ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not generate master secret: %s\n",
		       tls, strerror ( rc ) );
		goto err_master;
	}

 err_master:
 err_send:
 err_prepend:
 err_build:
 err_exchange:
	free ( builder.data );
	return rc;
}

/**
 * Transmit Certificate Verify record
 *
 * @v tls		TLS connection
 * @ret rc		Return status code
 */
static int tls_send_certificate_verify ( struct tls_connection *tls ) {
	struct digest_algorithm *digest = tls->key.digest;
	struct x509_certificate *cert = x509_first ( tls->client.chain );
	struct pubkey_algorithm *pubkey = cert->signature_algorithm->pubkey;
	struct asn1_cursor *key = privkey_cursor ( tls->client.key );
	struct tls_signature_hash_algorithm *sig_hash = NULL;
	struct asn1_builder builder = { NULL, 0 };
	size_t digestsize = digest->digestsize;
	uint8_t tbshash[digestsize];
	int rc;

	/* TLSv1.2 and later use explicit algorithm identifiers */
	if ( tls_version ( tls, TLS_VERSION_TLS_1_2 ) ) {
		sig_hash = tls_signature_hash_algorithm ( pubkey, digest );
		if ( ! sig_hash ) {
			DBGC ( tls, "TLS %p could not identify (%s,%s) "
			       "signature and hash algorithm\n", tls,
			       pubkey->name, digest->name );
			rc = -ENOTSUP_SIG_HASH;
			goto err_sig_hash;
		}
	}

	/* Generate digest */
	if ( ( rc = tlskey_tbshash ( &tls->key, &tls_client, digest, NULL, 0,
				     tbshash ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not generate CertificateVerify "
		       "digest: %s\n", tls, strerror ( rc ) );
		goto err_tbshash;
	}

	/* Sign digest */
	if ( ( rc = pubkey_sign ( pubkey, key, digest, tbshash,
				  &builder ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not sign %s digest using %s client "
		       "private key: %s\n", tls, digest->name, pubkey->name,
		       strerror ( rc ) );
		goto err_pubkey_sign;
	}

	/* Construct Certificate Verify record */
	{
		int use_sig_hash = ( ( sig_hash == NULL ) ? 0 : 1 );
		struct {
			uint32_t type_length;
			uint16_t sig_hash[use_sig_hash];
			uint16_t signature_len;
		} __attribute__ (( packed )) header;

		header.type_length = ( cpu_to_le32 ( TLS_CERTIFICATE_VERIFY ) |
				       htonl ( builder.len +
					       sizeof ( header ) -
					       sizeof ( header.type_length )));
		if ( use_sig_hash ) {
			memcpy ( &header.sig_hash[0], &sig_hash->code,
				 sizeof ( header.sig_hash[0] ) );
		}
		header.signature_len = htons ( builder.len );

		if ( ( rc = asn1_prepend_raw ( &builder, &header,
					       sizeof ( header ) ) ) != 0 ) {
			DBGC ( tls, "TLS %p could not construct Certificate "
			       "Verify: %s\n", tls, strerror ( rc ) );
			goto err_prepend;
		}
	}

	/* Transmit record */
	if ( ( rc = tls_send_handshake ( tls, builder.data,
					 builder.len ) ) != 0 ) {
		goto err_send;
	}

 err_send:
 err_prepend:
 err_pubkey_sign:
 err_tbshash:
 err_sig_hash:
	zfree ( builder.data );
	return rc;
}

/**
 * Transmit Change Cipher record
 *
 * @v tls		TLS connection
 * @ret rc		Return status code
 */
static int tls_send_change_cipher ( struct tls_connection *tls ) {
	static const struct {
		uint8_t spec;
	} __attribute__ (( packed )) change_cipher = {
		.spec = TLS_CHANGE_CIPHER_SPEC,
	};

	return tls_send_plaintext ( tls, TLS_TYPE_CHANGE_CIPHER,
				    &change_cipher, sizeof ( change_cipher ) );
}

/**
 * Transmit Finished record
 *
 * @v tls		TLS connection
 * @ret rc		Return status code
 */
static int tls_send_finished ( struct tls_connection *tls ) {
	struct {
		uint32_t type_length;
		uint8_t verify_data[ sizeof ( tls->verify.client ) ];
	} __attribute__ (( packed )) finished;
	int rc;

	/* Construct client verification data */
	if ( ( rc = tlskey_verify ( &tls->key, &tls_client,
				    tls->verify.client,
				    sizeof ( tls->verify.client ) ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not generate client verification: "
		       "%s\n", tls, strerror ( rc ) );
		return rc;
	}

	/* Construct record */
	memset ( &finished, 0, sizeof ( finished ) );
	finished.type_length = ( cpu_to_le32 ( TLS_FINISHED ) |
				 htonl ( sizeof ( finished ) -
					 sizeof ( finished.type_length ) ) );
	memcpy ( finished.verify_data, tls->verify.client,
		 sizeof ( finished.verify_data ) );

	/* Transmit record */
	if ( ( rc = tls_send_handshake ( tls, &finished,
					 sizeof ( finished ) ) ) != 0 )
		return rc;

	/* Mark client as finished */
	pending_put ( &tls->client.negotiation );

	/* If server has finished, then establish the secure channel */
	if ( ( ! is_pending ( &tls->server.negotiation ) ) &&
	     ( ( rc = tls_establish ( tls ) ) != 0 ) ) {
		return rc;
	}

	return 0;
}

/**
 * Transmit Alert record
 *
 * @v tls		TLS connection
 * @v level		Alert level
 * @v description	Alert description
 * @ret rc		Return status code
 */
static int tls_send_alert ( struct tls_connection *tls, unsigned int level,
			    unsigned int description ) {
	const struct {
		uint8_t level;
		uint8_t description;
	} __attribute__ (( packed )) alert = {
		.level = level,
		.description = description,
	};

	/* Send record */
	return tls_send_plaintext ( tls, TLS_TYPE_ALERT, &alert,
				    sizeof ( alert ) );
}

/**
 * Receive new Change Cipher record
 *
 * @v tls		TLS connection
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int tls_new_change_cipher ( struct tls_connection *tls,
				   struct io_buffer *iobuf ) {
	const struct {
		uint8_t spec;
	} __attribute__ (( packed )) *change_cipher = iobuf->data;
	size_t len = iob_len ( iobuf );
	int rc;

	/* Sanity check */
	if ( ( sizeof ( *change_cipher ) != len ) ||
	     ( change_cipher->spec != TLS_CHANGE_CIPHER_SPEC ) ) {
		DBGC ( tls, "TLS %p received invalid Change Cipher\n", tls );
		DBGC_HD ( tls, change_cipher, len );
		return -EINVAL_CHANGE_CIPHER;
	}
	iob_pull ( iobuf, sizeof ( *change_cipher ) );

	/* Change receive cipher spec */
	if ( ( rc = tls_change_cipher ( tls, &tls->rx.cipherspec,
					&tls->channel.rx ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not activate RX cipher: %s\n",
		       tls, strerror ( rc ) );
		return rc;
	}
	tls->rx.seq = ~( ( uint64_t ) 0 );

	return 0;
}

/**
 * Receive new Alert record
 *
 * @v tls		TLS connection
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int tls_new_alert ( struct tls_connection *tls,
			   struct io_buffer *iobuf ) {
	const struct {
		uint8_t level;
		uint8_t description;
		char next[0];
	} __attribute__ (( packed )) *alert = iobuf->data;
	size_t len = iob_len ( iobuf );

	/* Sanity check */
	if ( sizeof ( *alert ) != len ) {
		DBGC ( tls, "TLS %p received overlength Alert\n", tls );
		DBGC_HD ( tls, alert, len );
		return -EINVAL_ALERT;
	}
	iob_pull ( iobuf, sizeof ( *alert ) );

	/* Handle alert */
	switch ( alert->level ) {
	case TLS_ALERT_WARNING:
		switch ( alert->description ) {
		case TLS_ALERT_CLOSE_NOTIFY:
			DBGC ( tls, "TLS %p closed by notification\n", tls );
			tls_close ( tls, 0 );
			break;
		default:
			DBGC ( tls, "TLS %p received warning alert %d\n",
			       tls, alert->description );
			break;
		}
		return 0;
	case TLS_ALERT_FATAL:
		DBGC ( tls, "TLS %p received fatal alert %d\n",
		       tls, alert->description );
		return -EPERM_ALERT;
	default:
		DBGC ( tls, "TLS %p received unknown alert level %d"
		       "(alert %d)\n", tls, alert->level, alert->description );
		return -EIO_ALERT;
	}
}

/**
 * Receive new Hello Request handshake record
 *
 * @v tls		TLS connection
 * @v data		Plaintext handshake record
 * @v len		Length of plaintext handshake record
 * @ret rc		Return status code
 */
static int tls_new_hello_request ( struct tls_connection *tls,
				   const void *data __unused,
				   size_t len __unused ) {

	/* Ignore if a handshake is in progress */
	if ( ! tls_ready ( tls ) ) {
		DBGC ( tls, "TLS %p ignoring Hello Request\n", tls );
		return 0;
	}

	/* Fail unless server supports secure renegotiation */
	if ( ! ( tls->secure_renegotiation && tls->extended_master_secret ) ) {
		DBGC ( tls, "TLS %p refusing to renegotiate insecurely\n",
		       tls );
		return -EPERM_RENEG_INSECURE;
	}

	/* Restart negotiation */
	tls_restart ( tls );

	return 0;
}

/**
 * Receive new Server Hello handshake record
 *
 * @v tls		TLS connection
 * @v data		Plaintext handshake record
 * @v len		Length of plaintext handshake record
 * @ret rc		Return status code
 */
static int tls_new_server_hello ( struct tls_connection *tls,
				  const void *data, size_t len ) {
	struct tls_session *session = tls->session;
	const struct {
		uint16_t version;
		struct tls_random random;
		uint8_t session_id_len;
		uint8_t session_id[0];
	} __attribute__ (( packed )) *hello_a = data;
	const uint8_t *session_id;
	const struct {
		uint16_t cipher_suite;
		uint8_t compression_method;
		char next[0];
	} __attribute__ (( packed )) *hello_b;
	const struct {
		uint16_t len;
		uint8_t data[0];
	} __attribute__ (( packed )) *exts;
	const struct {
		uint16_t type;
		uint16_t len;
		uint8_t data[0];
	} __attribute__ (( packed )) *ext;
	const struct {
		uint8_t len;
		uint8_t data[0];
	} __attribute__ (( packed )) *reneg = NULL;
	const struct {
		uint8_t data[0];
	} __attribute__ (( packed )) *ems = NULL;
	uint16_t version;
	size_t exts_len;
	size_t ext_len;
	size_t remaining;
	int rc;

	/* Parse header */
	if ( ( sizeof ( *hello_a ) > len ) ||
	     ( hello_a->session_id_len > ( len - sizeof ( *hello_a ) ) ) ||
	     ( sizeof ( *hello_b ) > ( len - sizeof ( *hello_a ) -
				       hello_a->session_id_len ) ) ) {
		DBGC ( tls, "TLS %p received underlength Server Hello\n", tls );
		DBGC_HD ( tls, data, len );
		return -EINVAL_HELLO;
	}
	session_id = hello_a->session_id;
	hello_b = ( ( void * ) ( session_id + hello_a->session_id_len ) );

	/* Parse extensions, if present */
	remaining = ( len - sizeof ( *hello_a ) - hello_a->session_id_len -
		      sizeof ( *hello_b ) );
	if ( remaining ) {

		/* Parse extensions length */
		exts = ( ( void * ) hello_b->next );
		if ( ( sizeof ( *exts ) > remaining ) ||
		     ( ( exts_len = ntohs ( exts->len ) ) >
		       ( remaining - sizeof ( *exts ) ) ) ) {
			DBGC ( tls, "TLS %p received underlength extensions\n",
			       tls );
			DBGC_HD ( tls, data, len );
			return -EINVAL_HELLO;
		}

		/* Parse extensions */
		for ( ext = ( ( void * ) exts->data ), remaining = exts_len ;
		      remaining ;
		      ext = ( ( ( void * ) ext ) + sizeof ( *ext ) + ext_len ),
			      remaining -= ( sizeof ( *ext ) + ext_len ) ) {

			/* Parse extension length */
			if ( ( sizeof ( *ext ) > remaining ) ||
			     ( ( ext_len = ntohs ( ext->len ) ) >
			       ( remaining - sizeof ( *ext ) ) ) ) {
				DBGC ( tls, "TLS %p received underlength "
				       "extension\n", tls );
				DBGC_HD ( tls, data, len );
				return -EINVAL_HELLO;
			}

			/* Record known extensions */
			switch ( ext->type ) {
			case htons ( TLS_RENEGOTIATION_INFO ) :
				reneg = ( ( void * ) ext->data );
				if ( ( sizeof ( *reneg ) > ext_len ) ||
				     ( reneg->len >
				       ( ext_len - sizeof ( *reneg ) ) ) ) {
					DBGC ( tls, "TLS %p received "
					       "underlength renegotiation "
					       "info\n", tls );
					DBGC_HD ( tls, data, len );
					return -EINVAL_HELLO;
				}
				break;
			case htons ( TLS_EXTENDED_MASTER_SECRET ) :
				ems = ( ( void * ) ext->data );
				break;
			}
		}
	}

	/* Check and store protocol version */
	version = ntohs ( hello_a->version );
	if ( version < TLS_VERSION_MIN ) {
		DBGC ( tls, "TLS %p does not support protocol version %d.%d\n",
		       tls, ( version >> 8 ), ( version & 0xff ) );
		return -ENOTSUP_VERSION;
	}
	if ( version > tls->version ) {
		DBGC ( tls, "TLS %p server attempted to illegally upgrade to "
		       "protocol version %d.%d\n",
		       tls, ( version >> 8 ), ( version & 0xff ) );
		return -EPROTO_VERSION;
	}
	tls->version = version;
	DBGC ( tls, "TLS %p using protocol version %d.%d\n",
	       tls, ( version >> 8 ), ( version & 0xff ) );

	/* Select cipher suite */
	if ( ( rc = tls_select_cipher ( tls, hello_b->cipher_suite ) ) != 0 )
		return rc;

	/* Add preceding Client Hello to handshake digest
	 *
	 * When the Client Hello was originally sent, the digest
	 * algorithm selected by the server's choice of cipher suite
	 * was not yet known.  This is the earliest point at which it
	 * can be incorporated into the handshake transcript digest.
	 */
	if ( ( rc = tls_client_hello ( tls, tls_add_handshake ) ) != 0 )
		return rc;

	/* Handle extended master secret */
	tls->extended_master_secret = ( !! ems );

	/* Check session ID */
	if ( hello_a->session_id_len &&
	     ( hello_a->session_id_len == session->id.len ) &&
	     ( memcmp ( session_id, session->id.data,
			session->id.len ) == 0 ) ) {

		/* Session ID match: resume session */
		if ( ( rc = tls_resume ( tls ) ) != 0 )
			return rc;

	} else if ( hello_a->session_id_len &&
		    ( hello_a->session_id_len <=
		      sizeof ( tls->new_id.data ) ) ) {

		/* Record new session ID */
		tls->new_id.len = hello_a->session_id_len;
		memcpy ( tls->new_id.data, session_id, tls->new_id.len );
		DBGC ( tls, "TLS %p new session ID:\n", tls );
		DBGC_HDA ( tls, 0, tls->new_id.data, tls->new_id.len );
	}

	/* Handle secure renegotiation */
	if ( tls->secure_renegotiation ) {

		/* Secure renegotiation is expected; verify data */
		if ( ( reneg == NULL ) ||
		     ( reneg->len != sizeof ( tls->verify ) ) ||
		     ( memcmp ( reneg->data, &tls->verify,
				sizeof ( tls->verify ) ) != 0 ) ) {
			DBGC ( tls, "TLS %p server failed secure "
			       "renegotiation\n", tls );
			return -EPERM_RENEG_VERIFY;
		}

	} else if ( reneg != NULL ) {

		/* Secure renegotiation is being enabled */
		if ( reneg->len != 0 ) {
			DBGC ( tls, "TLS %p server provided non-empty initial "
			       "renegotiation\n", tls );
			return -EPERM_RENEG_VERIFY;
		}
		tls->secure_renegotiation = 1;
	}

	return 0;
}

/**
 * Receive New Session Ticket handshake record
 *
 * @v tls		TLS connection
 * @v data		Plaintext handshake record
 * @v len		Length of plaintext handshake record
 * @ret rc		Return status code
 */
static int tls_new_session_ticket ( struct tls_connection *tls,
				    const void *data, size_t len ) {
	const struct {
		uint32_t lifetime;
		uint16_t len;
		uint8_t ticket[0];
	} __attribute__ (( packed )) *new_session_ticket = data;
	size_t ticket_len;

	/* Parse header */
	if ( sizeof ( *new_session_ticket ) > len ) {
		DBGC ( tls, "TLS %p received underlength New Session Ticket\n",
		       tls );
		DBGC_HD ( tls, data, len );
		return -EINVAL_TICKET;
	}
	ticket_len = ntohs ( new_session_ticket->len );
	if ( ticket_len > ( len - sizeof ( *new_session_ticket ) ) ) {
		DBGC ( tls, "TLS %p received overlength New Session Ticket\n",
		       tls );
		DBGC_HD ( tls, data, len );
		return -EINVAL_TICKET;
	}

	/* Free any unapplied new session ticket */
	zfree ( tls->new_ticket.data );
	tls->new_ticket.data = NULL;
	tls->new_ticket.len = 0;

	/* Record ticket */
	tls->new_ticket.data = malloc ( ticket_len );
	if ( ! tls->new_ticket.data )
		return -ENOMEM;
	memcpy ( tls->new_ticket.data, new_session_ticket->ticket,
		 ticket_len );
	tls->new_ticket.len = ticket_len;
	DBGC ( tls, "TLS %p new session ticket:\n", tls );
	DBGC_HDA ( tls, 0, tls->new_ticket.data, tls->new_ticket.len );

	return 0;
}

/**
 * Parse certificate chain
 *
 * @v tls		TLS connection
 * @v data		Certificate chain
 * @v len		Length of certificate chain
 * @ret rc		Return status code
 */
static int tls_parse_chain ( struct tls_connection *tls,
			     const void *data, size_t len ) {
	struct x509_certificate *cert;
	size_t remaining = len;
	int rc;

	/* Free any existing certificate chain */
	x509_chain_put ( tls->server.chain );
	tls->server.chain = NULL;

	/* Create certificate chain */
	tls->server.chain = x509_alloc_chain();
	if ( ! tls->server.chain ) {
		rc = -ENOMEM_CHAIN;
		goto err_alloc_chain;
	}

	/* Add certificates to chain */
	while ( remaining ) {
		const struct {
			tls24_t length;
			uint8_t data[0];
		} __attribute__ (( packed )) *certificate = data;
		size_t certificate_len;
		size_t record_len;

		/* Parse header */
		if ( sizeof ( *certificate ) > remaining ) {
			DBGC ( tls, "TLS %p underlength certificate:\n", tls );
			DBGC_HDA ( tls, 0, data, remaining );
			rc = -EINVAL_CERTIFICATE;
			goto err_underlength;
		}
		certificate_len = tls_uint24 ( &certificate->length );
		if ( certificate_len > ( remaining - sizeof ( *certificate ) )){
			DBGC ( tls, "TLS %p overlength certificate:\n", tls );
			DBGC_HDA ( tls, 0, data, remaining );
			rc = -EINVAL_CERTIFICATE;
			goto err_overlength;
		}
		record_len = ( sizeof ( *certificate ) + certificate_len );

		/* Add certificate to chain */
		if ( ( rc = x509_append_raw ( tls->server.chain,
					      certificate->data,
					      certificate_len ) ) != 0 ) {
			DBGC ( tls, "TLS %p could not append certificate: %s\n",
			       tls, strerror ( rc ) );
			DBGC_HDA ( tls, 0, data, remaining );
			goto err_parse;
		}
		cert = x509_last ( tls->server.chain );
		DBGC ( tls, "TLS %p found certificate %s\n",
		       tls, x509_name ( cert ) );

		/* Move to next certificate in list */
		data += record_len;
		remaining -= record_len;
	}

	return 0;

 err_parse:
 err_overlength:
 err_underlength:
	x509_chain_put ( tls->server.chain );
	tls->server.chain = NULL;
 err_alloc_chain:
	return rc;
}

/**
 * Receive new Certificate handshake record
 *
 * @v tls		TLS connection
 * @v data		Plaintext handshake record
 * @v len		Length of plaintext handshake record
 * @ret rc		Return status code
 */
static int tls_new_certificate ( struct tls_connection *tls,
				 const void *data, size_t len ) {
	const struct {
		tls24_t length;
		uint8_t certificates[0];
	} __attribute__ (( packed )) *certificate = data;
	size_t certificates_len;
	int rc;

	/* Parse header */
	if ( sizeof ( *certificate ) > len ) {
		DBGC ( tls, "TLS %p received underlength Server Certificate\n",
		       tls );
		DBGC_HD ( tls, data, len );
		return -EINVAL_CERTIFICATES;
	}
	certificates_len = tls_uint24 ( &certificate->length );
	if ( certificates_len > ( len - sizeof ( *certificate ) ) ) {
		DBGC ( tls, "TLS %p received overlength Server Certificate\n",
		       tls );
		DBGC_HD ( tls, data, len );
		return -EINVAL_CERTIFICATES;
	}

	/* Parse certificate chain */
	if ( ( rc = tls_parse_chain ( tls, certificate->certificates,
				      certificates_len ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Receive new Server Key Exchange handshake record
 *
 * @v tls		TLS connection
 * @v data		Plaintext handshake record
 * @v len		Length of plaintext handshake record
 * @ret rc		Return status code
 */
static int tls_new_server_key_exchange ( struct tls_connection *tls,
					 const void *data, size_t len ) {
	struct tls_cipherspec *cipherspec = &tls->tx.cipherspec.pending;
	struct tls_cipher_suite *suite = cipherspec->suite;
	struct tls_key_exchange_parameters params;
	struct tls_signature_hash_algorithm *sig_hash;
	struct x509_certificate *cert;
	struct pubkey_algorithm *pubkey;
	struct digest_algorithm *digest;
	int use_sig_hash = tls_version ( tls, TLS_VERSION_TLS_1_2 );
	const struct {
		uint16_t sig_hash[use_sig_hash];
		uint16_t signature_len;
		uint8_t signature[0];
	} __attribute__ (( packed )) *sig;
	struct asn1_cursor signature;
	size_t remaining;
	int rc;

	/* Identify server certificate */
	cert = x509_first ( tls->server.chain );
	if ( ! cert ) {
		DBGC ( tls, "TLS %p has no server certificate\n", tls );
		return -ENOENT_CERT;
	}

	/* Parse parameters */
	if ( ! suite->exchange->parse ) {
		DBGC ( tls, "TLS %p received unexpected ServerKeyExchange:\n",
		       tls );
		DBGC_HDA ( tls, 0, data, len );
		return -EINVAL_KEY_EXCHANGE;
	}
	if ( ( rc = suite->exchange->parse ( tls, data, len, &params ) ) != 0)
		return rc;
	DBGC ( tls, "TLS %p using named group %s-%s\n",
	       tls, suite->exchange->name, params.exchange->name );

	/* Signature follows parameters */
	assert ( params.len <= len );
	sig = ( data + params.len );
	remaining = ( len - params.len );

	/* Parse signature from ServerKeyExchange */
	if ( ( sizeof ( *sig ) > remaining ) ||
	     ( ntohs ( sig->signature_len ) > ( remaining -
						sizeof ( *sig ) ) ) ) {
		DBGC ( tls, "TLS %p received underlength ServerKeyExchange\n",
		       tls );
		DBGC_HDA ( tls, 0, data, len );
		return -EINVAL_KEY_EXCHANGE;
	}
	signature.data = sig->signature;
	signature.len = ntohs ( sig->signature_len );

	/* Identify signature and hash algorithm */
	if ( use_sig_hash ) {
		sig_hash = tls_find_signature_hash ( sig->sig_hash[0] );
		if ( ! sig_hash ) {
			DBGC ( tls, "TLS %p unsupported signature hash "
			       "%#04x\n", tls, sig->sig_hash[0] );
			return -ENOTSUP_SIG_HASH;
		}
		pubkey = sig_hash->pubkey;
		digest = sig_hash->digest;
		DBGC ( tls, "TLS %p using signature hash %s-%s\n",
		       tls, pubkey->name, digest->name );
		if ( sig_hash->algorithm !=
		     cert->subject.public_key.algorithm ) {
			DBGC ( tls, "TLS %p cannot use %s public key\n", tls,
			       cert->subject.public_key.algorithm->name );
			return -EPERM_KEY_EXCHANGE;
		}
	} else {
		pubkey = cipherspec->suite->pubkey;
		digest = &md5_sha1_algorithm;
	}

	/* Generate pre-master secret */
	if ( ( rc = tls_key_agree ( tls, params.exchange, params.partner,
				    params.partner_len ) ) != 0 ) {
		return rc;
	}

	/* Verify signature */
	{
		uint8_t tbshash[digest->digestsize];

		/* Calculate digest */
		if ( ( rc = tlskey_tbshash ( &tls->key, &tls_server,
					     digest, data, params.len,
					     tbshash ) ) != 0 ) {
			DBGC ( tls, "TLS %p could not generate "
			       "ServerKeyExchange digest: %s\n",
			       tls, strerror ( rc ) );
			return rc;
		}

		/* Verify signature to bind pre-master secret */
		if ( ( rc = channel_bind_verify ( &tls->channel, cert,
						  pubkey, digest, tbshash,
						  &signature ) ) != 0 ) {
			DBGC ( tls, "TLS %p ServerKeyExchange failed "
			       "verification: %s\n", tls, strerror ( rc ) );
			DBGC_HDA ( tls, 0, data, len );
			return rc;
		}
	}

	/* Record key exchange algorithm for sending ClientKeyExchange */
	tls->exchange = params.exchange;

	return 0;
}

/**
 * Receive new Certificate Request handshake record
 *
 * @v tls		TLS connection
 * @v data		Plaintext handshake record
 * @v len		Length of plaintext handshake record
 * @ret rc		Return status code
 */
static int tls_new_certificate_request ( struct tls_connection *tls,
					 const void *data __unused,
					 size_t len __unused ) {
	struct x509_certificate *cert;
	int rc;

	/* We can only send a single certificate, so there is no point
	 * in parsing the Certificate Request.
	 */

	/* Free any existing client certificate chain */
	x509_chain_put ( tls->client.chain );
	tls->client.chain = NULL;

	/* Create client certificate chain */
	tls->client.chain = x509_alloc_chain();
	if ( ! tls->client.chain ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Determine client certificate to be sent, if any */
	cert = x509_find_key ( NULL, tls->client.key );
	if ( cert ) {
		DBGC ( tls, "TLS %p selected client certificate %s\n",
		       tls, x509_name ( cert ) );

		/* Append client certificate to chain */
		if ( ( rc = x509_append ( tls->client.chain, cert ) ) != 0 )
			goto err_append;

		/* Append any relevant issuer certificates */
		if ( ( rc = x509_auto_append ( tls->client.chain,
					       &certstore ) ) != 0 )
			goto err_auto_append;
	} else {

		/* Send an empty certificate chain */
		DBGC ( tls, "TLS %p could not find certificate corresponding "
		       "to private key\n", tls );
	}

	return 0;

 err_auto_append:
 err_append:
	x509_chain_put ( tls->client.chain );
	tls->client.chain = NULL;
 err_alloc:
	return rc;
}

/**
 * Receive new Server Hello Done handshake record
 *
 * @v tls		TLS connection
 * @v data		Plaintext handshake record
 * @v len		Length of plaintext handshake record
 * @ret rc		Return status code
 */
static int tls_new_server_hello_done ( struct tls_connection *tls,
				       const void *data, size_t len ) {
	const struct {
		char next[0];
	} __attribute__ (( packed )) *hello_done = data;
	int rc;

	/* Sanity check */
	if ( sizeof ( *hello_done ) != len ) {
		DBGC ( tls, "TLS %p received overlength Server Hello Done\n",
		       tls );
		DBGC_HD ( tls, data, len );
		return -EINVAL_HELLO_DONE;
	}

	/* Begin certificate validation */
	if ( ( rc = create_validator ( &tls->server.validator,
				       tls->server.chain,
				       tls->server.root ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not start certificate validation: "
		       "%s\n", tls, strerror ( rc ) );
		return rc;
	}
	pending_get ( &tls->server.validation );

	return 0;
}

/**
 * Receive new Finished handshake record
 *
 * @v tls		TLS connection
 * @v data		Plaintext handshake record
 * @v len		Length of plaintext handshake record
 * @ret rc		Return status code
 */
static int tls_new_finished ( struct tls_connection *tls,
			      const void *data, size_t len ) {
	int rc;

	/* Confirm peer identity */
	if ( ( rc = channel_confirm ( &tls->channel, data, len ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not confirm peer identity: %s\n",
		       tls, strerror ( rc ) );
		return rc;
	}

	/* Mark server as finished */
	pending_put ( &tls->server.negotiation );

	/* If client has finished, then establish the secure channel */
	if ( ( ! is_pending ( &tls->client.negotiation ) ) &&
	     ( ( rc = tls_establish ( tls ) ) != 0 ) ) {
		return rc;
	}

	/* If we are resuming a session (i.e. if the server Finished
	 * arrives before the client Finished is sent), then schedule
	 * transmission of Change Cipher and Finished.
	 */
	if ( is_pending ( &tls->client.negotiation ) ) {
		tls->tx.pending |= ( TLS_TX_CHANGE_CIPHER | TLS_TX_FINISHED );
		tls_tx_resume ( tls );
	}

	return 0;
}

/**
 * Receive new Handshake record
 *
 * @v tls		TLS connection
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int tls_new_handshake ( struct tls_connection *tls,
			       struct io_buffer *iobuf ) {
	size_t remaining;
	int rc;

	while ( ( remaining = iob_len ( iobuf ) ) ) {
		const struct {
			uint8_t type;
			tls24_t length;
			uint8_t payload[0];
		} __attribute__ (( packed )) *handshake = iobuf->data;
		const void *payload;
		size_t payload_len;
		size_t record_len;

		/* Parse header */
		if ( sizeof ( *handshake ) > remaining ) {
			/* Leave remaining fragment unconsumed */
			break;
		}
		payload_len = tls_uint24 ( &handshake->length );
		if ( payload_len > ( remaining - sizeof ( *handshake ) ) ) {
			/* Leave remaining fragment unconsumed */
			break;
		}
		payload = &handshake->payload;
		record_len = ( sizeof ( *handshake ) + payload_len );

		/* Handle payload */
		switch ( handshake->type ) {
		case TLS_HELLO_REQUEST:
			rc = tls_new_hello_request ( tls, payload,
						     payload_len );
			break;
		case TLS_SERVER_HELLO:
			rc = tls_new_server_hello ( tls, payload, payload_len );
			break;
		case TLS_NEW_SESSION_TICKET:
			rc = tls_new_session_ticket ( tls, payload,
						      payload_len );
			break;
		case TLS_CERTIFICATE:
			rc = tls_new_certificate ( tls, payload, payload_len );
			break;
		case TLS_SERVER_KEY_EXCHANGE:
			rc = tls_new_server_key_exchange ( tls, payload,
							   payload_len );
			break;
		case TLS_CERTIFICATE_REQUEST:
			rc = tls_new_certificate_request ( tls, payload,
							   payload_len );
			break;
		case TLS_SERVER_HELLO_DONE:
			rc = tls_new_server_hello_done ( tls, payload,
							 payload_len );
			break;
		case TLS_FINISHED:
			rc = tls_new_finished ( tls, payload, payload_len );
			break;
		default:
			DBGC ( tls, "TLS %p ignoring handshake type %d\n",
			       tls, handshake->type );
			rc = 0;
			break;
		}

		/* Add to handshake digest (except for Hello Requests,
		 * which are explicitly excluded).
		 */
		if ( handshake->type != TLS_HELLO_REQUEST )
			tls_add_handshake ( tls, handshake, record_len );

		/* Abort on failure */
		if ( rc != 0 )
			return rc;

		/* Move to next handshake record */
		iob_pull ( iobuf, record_len );
	}

	return 0;
}

/**
 * Receive new unknown record
 *
 * @v tls		TLS connection
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int tls_new_unknown ( struct tls_connection *tls __unused,
			     struct io_buffer *iobuf ) {

	/* RFC4346 says that we should just ignore unknown record types */
	iob_pull ( iobuf, iob_len ( iobuf ) );
	return 0;
}

/**
 * Receive new data record
 *
 * @v tls		TLS connection
 * @v rx_data		List of received data buffers
 * @ret rc		Return status code
 */
static int tls_new_data ( struct tls_connection *tls,
			  struct list_head *rx_data ) {
	struct io_buffer *iobuf;
	int rc;

	/* Fail unless we are ready to receive data */
	if ( ! tls_ready ( tls ) )
		return -ENOTCONN;

	/* Deliver each I/O buffer in turn */
	while ( ( iobuf = list_first_entry ( rx_data, struct io_buffer,
					     list ) ) ) {
		list_del ( &iobuf->list );
		if ( ( rc = xfer_deliver_iob ( &tls->plainstream,
					       iobuf ) ) != 0 ) {
			DBGC ( tls, "TLS %p could not deliver data: "
			       "%s\n", tls, strerror ( rc ) );
			return rc;
		}
	}

	return 0;
}

/**
 * Receive new record
 *
 * @v tls		TLS connection
 * @v type		Record type
 * @v rx_data		List of received data buffers
 * @ret rc		Return status code
 */
static int tls_new_record ( struct tls_connection *tls, unsigned int type,
			    struct list_head *rx_data ) {
	int ( * handler ) ( struct tls_connection *tls,
			    struct io_buffer *iobuf );
	struct io_buffer *tmp = NULL;
	struct io_buffer **iobuf;
	int rc;

	/* Deliver data records as-is to the plainstream interface */
	if ( type == TLS_TYPE_DATA )
		return tls_new_data ( tls, rx_data );

	/* Determine handler and fragment buffer */
	iobuf = &tmp;
	switch ( type ) {
	case TLS_TYPE_CHANGE_CIPHER:
		handler = tls_new_change_cipher;
		break;
	case TLS_TYPE_ALERT:
		handler = tls_new_alert;
		break;
	case TLS_TYPE_HANDSHAKE:
		handler = tls_new_handshake;
		iobuf = &tls->rx.handshake;
		break;
	default:
		DBGC ( tls, "TLS %p unknown record type %d\n", tls, type );
		handler = tls_new_unknown;
		break;
	}

	/* Merge into a single I/O buffer */
	if ( *iobuf )
		list_add ( &(*iobuf)->list, rx_data );
	*iobuf = iob_concatenate ( rx_data );
	if ( ! *iobuf ) {
		DBGC ( tls, "TLS %p could not concatenate non-data record "
		       "type %d\n", tls, type );
		rc = -ENOMEM_RX_CONCAT;
		goto err_concatenate;
	}

	/* Handle record */
	if ( ( rc = handler ( tls, *iobuf ) ) != 0 )
		goto err_handle;

	/* Discard I/O buffer if empty */
	if ( ! iob_len ( *iobuf ) ) {
		free_iob ( *iobuf );
		*iobuf = NULL;
	}

	/* Sanity check */
	assert ( tmp == NULL );

	return 0;

 err_handle:
	free_iob ( *iobuf );
	*iobuf = NULL;
 err_concatenate:
	return rc;
}

/******************************************************************************
 *
 * Record encryption/decryption
 *
 ******************************************************************************
 */

/**
 * Initialise HMAC
 *
 * @v cipherspec	Cipher specification
 * @v ctx		Context
 * @v authhdr		Authentication header
 */
static void tls_hmac_init ( struct tls_cipherspec *cipherspec, void *ctx,
			    struct tls_auth_header *authhdr ) {
	struct tls_cipher_suite *suite = cipherspec->suite;
	struct digest_algorithm *digest = suite->digest;

	hmac_init ( digest, ctx, cipherspec->mac_secret, suite->mac_len );
	hmac_update ( digest, ctx, authhdr, sizeof ( *authhdr ) );
}

/**
 * Update HMAC
 *
 * @v cipherspec	Cipher specification
 * @v ctx		Context
 * @v data		Data
 * @v len		Length of data
 */
static void tls_hmac_update ( struct tls_cipherspec *cipherspec, void *ctx,
			      const void *data, size_t len ) {
	struct digest_algorithm *digest = cipherspec->suite->digest;

	hmac_update ( digest, ctx, data, len );
}

/**
 * Finalise HMAC
 *
 * @v cipherspec	Cipher specification
 * @v ctx		Context
 * @v mac		HMAC to fill in
 */
static void tls_hmac_final ( struct tls_cipherspec *cipherspec, void *ctx,
			     void *hmac ) {
	struct digest_algorithm *digest = cipherspec->suite->digest;

	hmac_final ( digest, ctx, hmac );
}

/**
 * Calculate HMAC
 *
 * @v cipherspec	Cipher specification
 * @v authhdr		Authentication header
 * @v data		Data
 * @v len		Length of data
 * @v mac		HMAC to fill in
 */
static void tls_hmac ( struct tls_cipherspec *cipherspec,
		       struct tls_auth_header *authhdr,
		       const void *data, size_t len, void *hmac ) {
	struct digest_algorithm *digest = cipherspec->suite->digest;
	uint8_t ctx[ hmac_ctxsize ( digest ) ];

	tls_hmac_init ( cipherspec, ctx, authhdr );
	tls_hmac_update ( cipherspec, ctx, data, len );
	tls_hmac_final ( cipherspec, ctx, hmac );
}

/**
 * Calculate HMAC over list of I/O buffers
 *
 * @v cipherspec	Cipher specification
 * @v authhdr		Authentication header
 * @v list		List of I/O buffers
 * @v mac		HMAC to fill in
 */
static void tls_hmac_list ( struct tls_cipherspec *cipherspec,
			    struct tls_auth_header *authhdr,
			    struct list_head *list, void *hmac ) {
	struct digest_algorithm *digest = cipherspec->suite->digest;
	uint8_t ctx[ hmac_ctxsize ( digest ) ];
	struct io_buffer *iobuf;

	tls_hmac_init ( cipherspec, ctx, authhdr );
	list_for_each_entry ( iobuf, list, list ) {
		tls_hmac_update ( cipherspec, ctx, iobuf->data,
				  iob_len ( iobuf ) );
	}
	tls_hmac_final ( cipherspec, ctx, hmac );
}

/**
 * Calculate maximum additional length required for transmitted record(s)
 *
 * @v tls		TLS connection
 * @v len		I/O buffer payload length
 * @ret reserve		Maximum additional length to reserve
 */
static size_t tls_iob_reserved ( struct tls_connection *tls, size_t len ) {
	struct tls_cipherspec *cipherspec = &tls->tx.cipherspec.active;
	struct tls_cipher_suite *suite = cipherspec->suite;
	struct cipher_algorithm *cipher = suite->cipher;
	struct tls_header *tlshdr;
	unsigned int count;
	size_t each;

	/* Calculate number of records (allowing for zero-length records) */
	count = ( len ? ( ( len + TLS_TX_BUFSIZE - 1 ) / TLS_TX_BUFSIZE ) : 1 );

	/* Calculate maximum additional length per record */
	each = ( sizeof ( *tlshdr ) + suite->record_iv_len + suite->mac_len +
		 ( is_block_cipher ( cipher ) ? cipher->blocksize : 0 ) +
		 cipher->authsize );

	/* Calculate maximum total additional length */
	return ( count * each );
}

/**
 * Allocate I/O buffer for transmitted record(s)
 *
 * @v tls		TLS connection
 * @v len		I/O buffer payload length
 * @ret iobuf		I/O buffer
 */
static struct io_buffer * tls_alloc_iob ( struct tls_connection *tls,
					  size_t len ) {
	struct io_buffer *iobuf;
	size_t reserve;

	/* Calculate maximum additional length to reserve */
	reserve = tls_iob_reserved ( tls, len );

	/* Allocate I/O buffer */
	iobuf = xfer_alloc_iob ( &tls->cipherstream, ( reserve + len ) );
	if ( ! iobuf )
		return NULL;

	/* Reserve space */
	iob_reserve ( iobuf, reserve );

	return iobuf;
}

/**
 * Send plaintext record(s)
 *
 * @v tls		TLS connection
 * @v type		Record type
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int tls_send_record ( struct tls_connection *tls, unsigned int type,
			     struct io_buffer *iobuf ) {
	struct tls_cipherspec *cipherspec = &tls->tx.cipherspec.active;
	struct tls_cipher_suite *suite = cipherspec->suite;
	struct cipher_algorithm *cipher = suite->cipher;
	struct digest_algorithm *digest = suite->digest;
	struct secure_pipe *pipe = &tls->channel.tx;
	struct {
		uint8_t fixed[suite->fixed_iv_len];
		uint8_t rec[suite->record_iv_len];
	} __attribute__ (( packed )) iv;
	struct tls_auth_header authhdr;
	struct tls_header *tlshdr;
	uint8_t mac[digest->digestsize];
	const void *plaintext;
	const void *encrypt;
	void *ciphertext;
	size_t record_len;
	size_t encrypt_len;
	size_t pad_len;
	size_t len;
	int rc;

	/* Sanity check */
	assert ( pipe->cipher == cipher );

	/* Record plaintext pointer and length */
	plaintext = iobuf->data;
	len = iob_len ( iobuf );

	/* Add to handshake digest if applicable */
	if ( type == TLS_TYPE_HANDSHAKE )
		tls_add_handshake ( tls, plaintext, len );

	/* Start constructing ciphertext at start of reserved space */
	iob_push ( iobuf, tls_iob_reserved ( tls, len ) );
	iob_unput ( iobuf, iob_len ( iobuf ) );

	/* Construct records */
	do {
		/* Limit length of this record (may be zero) */
		record_len = len;
		if ( record_len > TLS_TX_BUFSIZE )
			record_len = TLS_TX_BUFSIZE;

		/* Construct authentication header */
		authhdr.seq = cpu_to_be64 ( tls->tx.seq );
		authhdr.header.type = type;
		authhdr.header.version = htons ( tls->version );
		authhdr.header.length = htons ( record_len );

		/* Construct and set initialisation vector */
		memcpy ( iv.fixed, cipherspec->fixed_iv, sizeof ( iv.fixed ) );
		channel_ephemeral ( &tls->channel, &authhdr,
				    sizeof ( authhdr ), iv.rec,
				    sizeof ( iv.rec ) );
		if ( ( rc = cipher_setiv ( cipher, pipe->ctx, &iv,
					   sizeof ( iv ) ) ) != 0 ) {
			DBGC ( tls, "TLS %p could not set TX IV: %s\n",
			       tls, strerror ( rc ) );
			goto err_setiv;
		}

		/* Process authentication data */
		if ( suite->mac_len ) {
			tls_hmac ( cipherspec, &authhdr, plaintext,
				   record_len, mac );
		}
		if ( is_auth_cipher ( cipher ) ) {
			cipher_encrypt ( cipher, pipe->ctx, &authhdr, NULL,
					 sizeof ( authhdr ) );
		}

		/* Calculate encryption length */
		encrypt_len = ( record_len + suite->mac_len );
		if ( is_block_cipher ( cipher ) ) {
			pad_len = ( ( ( cipher->blocksize - 1 ) &
				      -( encrypt_len + 1 ) ) + 1 );
		} else {
			pad_len = 0;
		}
		encrypt_len += pad_len;

		/* Add record header */
		tlshdr = iob_put ( iobuf, sizeof ( *tlshdr ) );
		tlshdr->type = type;
		tlshdr->version = htons ( tls->version );
		tlshdr->length = htons ( sizeof ( iv.rec ) + encrypt_len +
					 cipher->authsize );

		/* Add record initialisation vector, if applicable */
		memcpy ( iob_put ( iobuf, sizeof ( iv.rec ) ), iv.rec,
			 sizeof ( iv.rec ) );

		/* Copy plaintext data if necessary */
		ciphertext = iob_put ( iobuf, record_len );
		assert ( ciphertext <= plaintext );
		if ( encrypt_len > record_len ) {
			memmove ( ciphertext, plaintext, record_len );
			encrypt = ciphertext;
		} else {
			encrypt = plaintext;
		}

		/* Add MAC, if applicable */
		memcpy ( iob_put ( iobuf, suite->mac_len ), mac,
			 suite->mac_len );

		/* Add padding, if applicable */
		memset ( iob_put ( iobuf, pad_len ), ( pad_len - 1 ), pad_len );

		/* Encrypt data and append authentication tag */
		DBGC2 ( tls, "Sending plaintext data:\n" );
		DBGC2_HDA ( tls, 0, encrypt, encrypt_len );
		cipher_encrypt ( cipher, pipe->ctx, encrypt, ciphertext,
				 encrypt_len );
		cipher_auth ( cipher, pipe->ctx,
			      iob_put ( iobuf, cipher->authsize ) );

		/* Move to next record */
		tls->tx.seq += 1;
		plaintext += record_len;
		len -= record_len;

	} while ( len );

	/* Send ciphertext */
	if ( ( rc = xfer_deliver_iob ( &tls->cipherstream,
				       iob_disown ( iobuf ) ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not deliver ciphertext: %s\n",
		       tls, strerror ( rc ) );
		goto err_deliver;
	}

	assert ( iobuf == NULL );
	return 0;

 err_deliver:
 err_setiv:
	free_iob ( iobuf );
	return rc;
}

/**
 * Send plaintext record
 *
 * @v tls		TLS connection
 * @v type		Record type
 * @v data		Plaintext record
 * @v len		Length of plaintext record
 * @ret rc		Return status code
 */
static int tls_send_plaintext ( struct tls_connection *tls, unsigned int type,
				const void *data, size_t len ) {
	struct io_buffer *iobuf;
	int rc;

	/* Allocate I/O buffer */
	iobuf = tls_alloc_iob ( tls, len );
	if ( ! iobuf )
		return -ENOMEM_TX_PLAINTEXT;
	memcpy ( iob_put ( iobuf, len ), data, len );

	/* Transmit I/O buffer */
	if ( ( rc = tls_send_record ( tls, type, iob_disown ( iobuf ) ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Verify block padding
 *
 * @v tls		TLS connection
 * @v iobuf		Last received I/O buffer
 * @ret len		Padding length, or negative error
 * @ret rc		Return status code
 */
static int tls_verify_padding ( struct tls_connection *tls,
				struct io_buffer *iobuf ) {
	uint8_t *padding;
	unsigned int pad;
	unsigned int i;
	size_t len;

	/* Extract and verify padding */
	padding = ( iobuf->tail - 1 );
	pad = *padding;
	len = ( pad + 1 );
	if ( len > iob_len ( iobuf ) ) {
		DBGC ( tls, "TLS %p received underlength padding\n", tls );
		DBGC_HD ( tls, iobuf->data, iob_len ( iobuf ) );
		return -EINVAL_PADDING;
	}
	for ( i = 0 ; i < pad ; i++ ) {
		if ( *(--padding) != pad ) {
			DBGC ( tls, "TLS %p received bad padding\n", tls );
			DBGC_HD ( tls, iobuf->data, iob_len ( iobuf ) );
			return -EINVAL_PADDING;
		}
	}

	return len;
}

/**
 * Receive new ciphertext record
 *
 * @v tls		TLS connection
 * @v tlshdr		Record header
 * @v rx_data		List of received data buffers
 * @ret rc		Return status code
 */
static int tls_new_ciphertext ( struct tls_connection *tls,
				struct tls_header *tlshdr,
				struct list_head *rx_data ) {
	struct tls_cipherspec *cipherspec = &tls->rx.cipherspec.active;
	struct tls_cipher_suite *suite = cipherspec->suite;
	struct cipher_algorithm *cipher = suite->cipher;
	struct digest_algorithm *digest = suite->digest;
	struct secure_pipe *pipe = &tls->channel.rx;
	size_t len = ntohs ( tlshdr->length );
	struct {
		uint8_t fixed[suite->fixed_iv_len];
		uint8_t record[suite->record_iv_len];
	} __attribute__ (( packed )) iv;
	struct tls_auth_header authhdr;
	uint8_t verify_mac[digest->digestsize];
	uint8_t verify_auth[cipher->authsize];
	struct io_buffer *first;
	struct io_buffer *last;
	struct io_buffer *iobuf;
	void *mac;
	void *auth;
	size_t check_len;
	int pad_len;
	int rc;

	/* Sanity check */
	assert ( pipe->cipher == cipher );

	/* Locate first and last data buffers */
	assert ( ! list_empty ( rx_data ) );
	first = list_first_entry ( rx_data, struct io_buffer, list );
	last = list_last_entry ( rx_data, struct io_buffer, list );

	/* Extract initialisation vector */
	if ( iob_len ( first ) < sizeof ( iv.record ) ) {
		DBGC ( tls, "TLS %p received underlength IV\n", tls );
		DBGC_HD ( tls, first->data, iob_len ( first ) );
		return -EINVAL_IV;
	}
	memcpy ( iv.fixed, cipherspec->fixed_iv, sizeof ( iv.fixed ) );
	memcpy ( iv.record, first->data, sizeof ( iv.record ) );
	iob_pull ( first, sizeof ( iv.record ) );
	len -= sizeof ( iv.record );

	/* Extract unencrypted authentication tag */
	if ( iob_len ( last ) < cipher->authsize ) {
		DBGC ( tls, "TLS %p received underlength authentication tag\n",
		       tls );
		DBGC_HD ( tls, last->data, iob_len ( last ) );
		return -EINVAL_MAC;
	}
	iob_unput ( last, cipher->authsize );
	len -= cipher->authsize;
	auth = last->tail;

	/* Construct authentication data */
	authhdr.seq = cpu_to_be64 ( tls->rx.seq );
	authhdr.header.type = tlshdr->type;
	authhdr.header.version = tlshdr->version;
	authhdr.header.length = htons ( len );

	/* Set initialisation vector */
	if ( ( rc = cipher_setiv ( cipher, pipe->ctx, &iv,
				   sizeof ( iv ) ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not set RX IV: %s\n",
		       tls, strerror ( rc ) );
		return rc;
	}

	/* Process authentication data, if applicable */
	if ( is_auth_cipher ( cipher ) ) {
		cipher_decrypt ( cipher, pipe->ctx, &authhdr,
				 NULL, sizeof ( authhdr ) );
	}

	/* Decrypt the received data */
	check_len = 0;
	list_for_each_entry ( iobuf, &tls->rx.data, list ) {
		cipher_decrypt ( cipher, pipe->ctx,
				 iobuf->data, iobuf->data, iob_len ( iobuf ) );
		check_len += iob_len ( iobuf );
	}
	assert ( check_len == len );

	/* Strip block padding, if applicable */
	if ( is_block_cipher ( cipher ) ) {
		pad_len = tls_verify_padding ( tls, last );
		if ( pad_len < 0 ) {
			/* Assume zero padding length to avoid timing attacks */
			pad_len = 0;
		}
		iob_unput ( last, pad_len );
		len -= pad_len;
	}

	/* Extract decrypted MAC */
	if ( iob_len ( last ) < suite->mac_len ) {
		DBGC ( tls, "TLS %p received underlength MAC\n", tls );
		DBGC_HD ( tls, last->data, iob_len ( last ) );
		return -EINVAL_MAC;
	}
	iob_unput ( last, suite->mac_len );
	len -= suite->mac_len;
	mac = last->tail;

	/* Dump received data */
	DBGC2 ( tls, "Received plaintext data:\n" );
	check_len = 0;
	list_for_each_entry ( iobuf, rx_data, list ) {
		DBGC2_HD ( tls, iobuf->data, iob_len ( iobuf ) );
		check_len += iob_len ( iobuf );
	}
	assert ( check_len == len );

	/* Generate MAC */
	authhdr.header.length = htons ( len );
	if ( suite->mac_len )
		tls_hmac_list ( cipherspec, &authhdr, rx_data, verify_mac );

	/* Generate authentication tag */
	cipher_auth ( cipher, pipe->ctx, verify_auth );

	/* Verify MAC */
	if ( memcmp ( mac, verify_mac, suite->mac_len ) != 0 ) {
		DBGC ( tls, "TLS %p failed MAC verification\n", tls );
		return -EINVAL_MAC;
	}

	/* Verify authentication tag */
	if ( memcmp ( auth, verify_auth, cipher->authsize ) != 0 ) {
		DBGC ( tls, "TLS %p failed authentication tag verification\n",
		       tls );
		return -EINVAL_MAC;
	}

	/* Process plaintext record */
	if ( ( rc = tls_new_record ( tls, tlshdr->type, rx_data ) ) != 0 )
		return rc;

	return 0;
}

/******************************************************************************
 *
 * Plaintext stream operations
 *
 ******************************************************************************
 */

/**
 * Check flow control window
 *
 * @v tls		TLS connection
 * @ret len		Length of window
 */
static size_t tls_plainstream_window ( struct tls_connection *tls ) {

	/* Block window unless we are ready to accept data */
	if ( ! tls_ready ( tls ) )
		return 0;

	return xfer_window ( &tls->cipherstream );
}

/**
 * Deliver datagram as raw data
 *
 * @v tls		TLS connection
 * @v iobuf		I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 */
static int tls_plainstream_deliver ( struct tls_connection *tls,
				     struct io_buffer *iobuf,
				     struct xfer_metadata *meta __unused ) {
	int rc;

	/* Refuse unless we are ready to accept data */
	if ( ! tls_ready ( tls ) ) {
		rc = -ENOTCONN;
		goto done;
	}

	/* Send data record */
	if ( ( rc = tls_send_record ( tls, TLS_TYPE_DATA,
				      iob_disown ( iobuf ) ) ) != 0 )
		goto done;

 done:
	free_iob ( iobuf );
	return rc;
}

/**
 * Report job progress
 *
 * @v tls		TLS connection
 * @v progress		Progress report to fill in
 * @ret ongoing_rc	Ongoing job status code (if known)
 */
static int tls_progress ( struct tls_connection *tls,
			  struct job_progress *progress ) {

	/* Return cipherstream or validator progress as applicable */
	if ( is_pending ( &tls->server.validation ) ) {
		return job_progress ( &tls->server.validator, progress );
	} else {
		return job_progress ( &tls->cipherstream, progress );
	}
}

/** TLS plaintext stream interface operations */
static struct interface_operation tls_plainstream_ops[] = {
	INTF_OP ( xfer_alloc_iob, struct tls_connection *, tls_alloc_iob ),
	INTF_OP ( xfer_deliver, struct tls_connection *,
		  tls_plainstream_deliver ),
	INTF_OP ( xfer_window, struct tls_connection *,
		  tls_plainstream_window ),
	INTF_OP ( job_progress, struct tls_connection *, tls_progress ),
	INTF_OP ( intf_close, struct tls_connection *, tls_close_alert ),
};

/** TLS plaintext stream interface descriptor */
static struct interface_descriptor tls_plainstream_desc =
	INTF_DESC_PASSTHRU ( struct tls_connection, plainstream,
			     tls_plainstream_ops, cipherstream );

/******************************************************************************
 *
 * Ciphertext stream operations
 *
 ******************************************************************************
 */

/**
 * Handle received TLS header
 *
 * @v tls		TLS connection
 * @ret rc		Returned status code
 */
static int tls_newdata_process_header ( struct tls_connection *tls ) {
	struct tls_cipherspec *cipherspec = &tls->rx.cipherspec.active;
	struct cipher_algorithm *cipher = cipherspec->suite->cipher;
	size_t iv_len = cipherspec->suite->record_iv_len;
	size_t data_len = ntohs ( tls->rx.header.length );
	size_t remaining = data_len;
	size_t frag_len;
	size_t reserve;
	struct io_buffer *iobuf;
	struct io_buffer *tmp;
	int rc;

	/* Sanity check */
	assert ( ( TLS_RX_BUFSIZE % cipher->alignsize ) == 0 );

	/* Calculate alignment reservation at start of first data buffer */
	reserve = ( ( -iv_len ) & ( cipher->alignsize - 1 ) );
	remaining += reserve;

	/* Allocate data buffers now that we know the length */
	assert ( list_empty ( &tls->rx.data ) );
	do {

		/* Calculate fragment length.  Ensure that no block is
		 * smaller than TLS_RX_MIN_BUFSIZE (by increasing the
		 * allocation length if necessary).
		 */
		frag_len = remaining;
		if ( frag_len > TLS_RX_BUFSIZE )
			frag_len = TLS_RX_BUFSIZE;
		remaining -= frag_len;
		if ( remaining < TLS_RX_MIN_BUFSIZE ) {
			frag_len += remaining;
			remaining = 0;
		}

		/* Allocate buffer */
		iobuf = alloc_iob_raw ( frag_len, TLS_RX_ALIGN, 0 );
		if ( ! iobuf ) {
			DBGC ( tls, "TLS %p could not allocate %zd of %zd "
			       "bytes for receive buffer\n", tls,
			       remaining, data_len );
			rc = -ENOMEM_RX_DATA;
			goto err;
		}

		/* Ensure tailroom is exactly what we asked for.  This
		 * will result in unaligned I/O buffers when the
		 * fragment length is unaligned, which can happen only
		 * before we switch to using a block cipher.
		 */
		iob_reserve ( iobuf, ( iob_tailroom ( iobuf ) - frag_len ) );

		/* Ensure first buffer length will be aligned to a
		 * multiple of the cipher alignment size after
		 * stripping the record IV.
		 */
		iob_reserve ( iobuf, reserve );
		reserve = 0;

		/* Add I/O buffer to list */
		list_add_tail ( &iobuf->list, &tls->rx.data );

	} while ( remaining );
	assert ( ! list_empty ( &tls->rx.data ) );

	/* Move to data state */
	tls->rx.state = TLS_RX_DATA;

	return 0;

 err:
	list_for_each_entry_safe ( iobuf, tmp, &tls->rx.data, list ) {
		list_del ( &iobuf->list );
		free_iob ( iobuf );
	}
	return rc;
}

/**
 * Handle received TLS data payload
 *
 * @v tls		TLS connection
 * @ret rc		Returned status code
 */
static int tls_newdata_process_data ( struct tls_connection *tls ) {
	struct io_buffer *iobuf;
	int rc;

	/* Move current buffer to end of list */
	iobuf = list_first_entry ( &tls->rx.data, struct io_buffer, list );
	list_del ( &iobuf->list );
	list_add_tail ( &iobuf->list, &tls->rx.data );

	/* Continue receiving data if any space remains */
	iobuf = list_first_entry ( &tls->rx.data, struct io_buffer, list );
	if ( iob_tailroom ( iobuf ) )
		return 0;

	/* Process record */
	if ( ( rc = tls_new_ciphertext ( tls, &tls->rx.header,
					 &tls->rx.data ) ) != 0 )
		return rc;

	/* Increment RX sequence number */
	tls->rx.seq += 1;

	/* Return to header state */
	assert ( list_empty ( &tls->rx.data ) );
	tls->rx.state = TLS_RX_HEADER;
	iob_unput ( &tls->rx.iobuf, sizeof ( tls->rx.header ) );

	return 0;
}

/**
 * Check flow control window
 *
 * @v tls		TLS connection
 * @ret len		Length of window
 */
static size_t tls_cipherstream_window ( struct tls_connection *tls ) {

	/* Open window until we are ready to accept data */
	if ( ! tls_ready ( tls ) )
		return -1UL;

	return xfer_window ( &tls->plainstream );
}

/**
 * Receive new ciphertext
 *
 * @v tls		TLS connection
 * @v iobuf		I/O buffer
 * @v meta		Data transfer metadat
 * @ret rc		Return status code
 */
static int tls_cipherstream_deliver ( struct tls_connection *tls,
				      struct io_buffer *iobuf,
				      struct xfer_metadata *xfer __unused ) {
	size_t frag_len;
	int ( * process ) ( struct tls_connection *tls );
	struct io_buffer *dest;
	int rc;

	while ( iob_len ( iobuf ) ) {

		/* Select buffer according to current state */
		switch ( tls->rx.state ) {
		case TLS_RX_HEADER:
			dest = &tls->rx.iobuf;
			process = tls_newdata_process_header;
			break;
		case TLS_RX_DATA:
			dest = list_first_entry ( &tls->rx.data,
						  struct io_buffer, list );
			assert ( dest != NULL );
			process = tls_newdata_process_data;
			break;
		default:
			assert ( 0 );
			rc = -EINVAL_RX_STATE;
			goto done;
		}

		/* Copy data portion to buffer */
		frag_len = iob_len ( iobuf );
		if ( frag_len > iob_tailroom ( dest ) )
			frag_len = iob_tailroom ( dest );
		memcpy ( iob_put ( dest, frag_len ), iobuf->data, frag_len );
		iob_pull ( iobuf, frag_len );

		/* Process data if buffer is now full */
		if ( iob_tailroom ( dest ) == 0 ) {
			if ( ( rc = process ( tls ) ) != 0 ) {
				tls_close_alert ( tls, rc );
				goto done;
			}
		}
	}
	rc = 0;

 done:
	free_iob ( iobuf );
	return rc;
}

/** TLS ciphertext stream interface operations */
static struct interface_operation tls_cipherstream_ops[] = {
	INTF_OP ( xfer_deliver, struct tls_connection *,
		  tls_cipherstream_deliver ),
	INTF_OP ( xfer_window, struct tls_connection *,
		  tls_cipherstream_window ),
	INTF_OP ( xfer_window_changed, struct tls_connection *,
		  tls_tx_resume ),
	INTF_OP ( intf_close, struct tls_connection *, tls_close ),
};

/** TLS ciphertext stream interface descriptor */
static struct interface_descriptor tls_cipherstream_desc =
	INTF_DESC_PASSTHRU ( struct tls_connection, cipherstream,
			     tls_cipherstream_ops, plainstream );

/******************************************************************************
 *
 * Certificate validator
 *
 ******************************************************************************
 */

/**
 * Handle certificate validation completion
 *
 * @v tls		TLS connection
 * @v rc		Reason for completion
 */
static void tls_validator_done ( struct tls_connection *tls, int rc ) {

	/* Mark validation as complete */
	pending_put ( &tls->server.validation );

	/* Close validator interface */
	intf_restart ( &tls->server.validator, rc );

	/* Check for validation failure */
	if ( rc != 0 ) {
		DBGC ( tls, "TLS %p certificate validation failed: %s\n",
		       tls, strerror ( rc ) );
		goto err;
	}
	DBGC ( tls, "TLS %p certificate validation succeeded\n", tls );

	/* Schedule transmission of applicable handshake messages */
	tls->tx.pending |= ( TLS_TX_CLIENT_KEY_EXCHANGE |
			     TLS_TX_CHANGE_CIPHER |
			     TLS_TX_FINISHED );
	if ( tls->client.chain ) {
		tls->tx.pending |= TLS_TX_CERTIFICATE;
		if ( ! list_empty ( &tls->client.chain->links ) )
			tls->tx.pending |= TLS_TX_CERTIFICATE_VERIFY;
	}
	tls_tx_resume ( tls );

	return;

 err:
	tls_close_alert ( tls, rc );
	return;
}

/** TLS certificate validator interface operations */
static struct interface_operation tls_validator_ops[] = {
	INTF_OP ( intf_close, struct tls_connection *, tls_validator_done ),
};

/** TLS certificate validator interface descriptor */
static struct interface_descriptor tls_validator_desc =
	INTF_DESC ( struct tls_connection, server.validator,
		    tls_validator_ops );

/******************************************************************************
 *
 * Controlling process
 *
 ******************************************************************************
 */

/**
 * TLS TX state machine
 *
 * @v tls		TLS connection
 */
static void tls_tx_step ( struct tls_connection *tls ) {
	struct tls_session *session = tls->session;
	struct tls_connection *conn;
	int rc;

	/* Wait for cipherstream to become ready */
	if ( ! xfer_window ( &tls->cipherstream ) )
		return;

	/* Send first pending transmission */
	if ( tls->tx.pending & TLS_TX_CLIENT_HELLO ) {
		/* Serialise server negotiations within a session, to
		 * provide a consistent view of session IDs and
		 * session tickets.
		 */
		list_for_each_entry ( conn, &session->conn, list ) {
			if ( conn == tls )
				break;
			if ( is_pending ( &conn->server.negotiation ) )
				return;
		}
		/* Send Client Hello */
		if ( ( rc = tls_send_client_hello ( tls ) ) != 0 ) {
			DBGC ( tls, "TLS %p could not send Client Hello: %s\n",
			       tls, strerror ( rc ) );
			goto err;
		}
		tls->tx.pending &= ~TLS_TX_CLIENT_HELLO;
	} else if ( tls->tx.pending & TLS_TX_CERTIFICATE ) {
		/* Send Certificate */
		if ( ( rc = tls_send_certificate ( tls ) ) != 0 ) {
			DBGC ( tls, "TLS %p could not send Certificate: %s\n",
			       tls, strerror ( rc ) );
			goto err;
		}
		tls->tx.pending &= ~TLS_TX_CERTIFICATE;
	} else if ( tls->tx.pending & TLS_TX_CLIENT_KEY_EXCHANGE ) {
		/* Send Client Key Exchange */
		if ( ( rc = tls_send_client_key_exchange ( tls ) ) != 0 ) {
			DBGC ( tls, "TLS %p could not send Client Key "
			       "Exchange: %s\n", tls, strerror ( rc ) );
			goto err;
		}
		tls->tx.pending &= ~TLS_TX_CLIENT_KEY_EXCHANGE;
	} else if ( tls->tx.pending & TLS_TX_CERTIFICATE_VERIFY ) {
		/* Send Certificate Verify */
		if ( ( rc = tls_send_certificate_verify ( tls ) ) != 0 ) {
			DBGC ( tls, "TLS %p could not send Certificate "
			       "Verify: %s\n", tls, strerror ( rc ) );
			goto err;
		}
		tls->tx.pending &= ~TLS_TX_CERTIFICATE_VERIFY;
	} else if ( tls->tx.pending & TLS_TX_CHANGE_CIPHER ) {
		/* Send Change Cipher, and then change the cipher in use */
		if ( ( rc = tls_send_change_cipher ( tls ) ) != 0 ) {
			DBGC ( tls, "TLS %p could not send Change Cipher: "
			       "%s\n", tls, strerror ( rc ) );
			goto err;
		}
		if ( ( rc = tls_change_cipher ( tls, &tls->tx.cipherspec,
						&tls->channel.tx ) ) != 0 ) {
			DBGC ( tls, "TLS %p could not activate TX cipher: "
			       "%s\n", tls, strerror ( rc ) );
			goto err;
		}
		tls->tx.seq = 0;
		tls->tx.pending &= ~TLS_TX_CHANGE_CIPHER;
	} else if ( tls->tx.pending & TLS_TX_FINISHED ) {
		/* Send Finished */
		if ( ( rc = tls_send_finished ( tls ) ) != 0 ) {
			DBGC ( tls, "TLS %p could not send Finished: %s\n",
			       tls, strerror ( rc ) );
			goto err;
		}
		tls->tx.pending &= ~TLS_TX_FINISHED;
	}

	/* Reschedule process if pending transmissions remain,
	 * otherwise send notification of a window change.
	 */
	if ( tls->tx.pending ) {
		tls_tx_resume ( tls );
	} else {
		xfer_window_changed ( &tls->plainstream );
	}

	return;

 err:
	tls_close_alert ( tls, rc );
}

/** TLS TX process descriptor */
static struct process_descriptor tls_process_desc =
	PROC_DESC_ONCE ( struct tls_connection, tx.process, tls_tx_step );

/******************************************************************************
 *
 * Instantiator
 *
 ******************************************************************************
 */

/**
 * Add TLS on an interface
 *
 * @v xfer		Data transfer interface
 * @v name		Host name
 * @v root		Root of trust (or NULL to use default)
 * @v key		Private key (or NULL to use default)
 * @ret rc		Return status code
 */
int add_tls ( struct interface *xfer, const char *name,
	      struct x509_root *root, struct private_key *key ) {
	struct tls_connection *tls;
	int rc;

	/* Allocate and initialise TLS structure */
	tls = malloc ( sizeof ( *tls ) );
	if ( ! tls ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	memset ( tls, 0, sizeof ( *tls ) );
	ref_init ( &tls->refcnt, free_tls );
	INIT_LIST_HEAD ( &tls->list );
	intf_init ( &tls->plainstream, &tls_plainstream_desc, &tls->refcnt );
	intf_init ( &tls->cipherstream, &tls_cipherstream_desc, &tls->refcnt );
	intf_init ( &tls->server.validator, &tls_validator_desc, &tls->refcnt );
	process_init_stopped ( &tls->tx.process, &tls_process_desc,
			       &tls->refcnt );
	tls->client.key = privkey_get ( key ? key : &private_key );
	tls->server.root = x509_root_get ( root ? root : &root_certificates );
	tls->version = TLS_VERSION_MAX;
	channel_init ( &tls->channel, &tls_channel_ops );
	tls_clear_digest ( tls );
	tls->tx.cipherspec.writer = &tls_client;
	tls_clear_cipher ( tls, &tls->tx.cipherspec.active );
	tls_clear_cipher ( tls, &tls->tx.cipherspec.pending );
	tls->rx.cipherspec.writer = &tls_server;
	tls_clear_cipher ( tls, &tls->rx.cipherspec.active );
	tls_clear_cipher ( tls, &tls->rx.cipherspec.pending );
	iob_populate ( &tls->rx.iobuf, &tls->rx.header, 0,
		       sizeof ( tls->rx.header ) );
	INIT_LIST_HEAD ( &tls->rx.data );

	/* Open secure channel */
	if ( ( rc = channel_open ( &tls->channel ) ) != 0 )
		goto err_channel;

	/* Find or create session */
	if ( ( rc = tls_session ( tls, name ) ) != 0 )
		goto err_session;
	list_add_tail ( &tls->list, &tls->session->conn );

	/* Start negotiation */
	tls_restart ( tls );

	/* Attach to parent interface, mortalise self, and return */
	intf_insert ( xfer, &tls->plainstream, &tls->cipherstream );
	ref_put ( &tls->refcnt );
	return 0;

 err_session:
	channel_close ( &tls->channel );
 err_channel:
	ref_put ( &tls->refcnt );
 err_alloc:
	return rc;
}

/* Drag in objects via add_tls() */
REQUIRING_SYMBOL ( add_tls );

/* Drag in crypto configuration */
REQUIRE_OBJECT ( config_crypto );
