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

/**
 * @file
 *
 * Transport Layer Security Protocol
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/pending.h>
#include <ipxe/hmac.h>
#include <ipxe/md5.h>
#include <ipxe/sha1.h>
#include <ipxe/sha256.h>
#include <ipxe/aes.h>
#include <ipxe/rsa.h>
#include <ipxe/iobuf.h>
#include <ipxe/xfer.h>
#include <ipxe/open.h>
#include <ipxe/x509.h>
#include <ipxe/privkey.h>
#include <ipxe/certstore.h>
#include <ipxe/rootcert.h>
#include <ipxe/rbg.h>
#include <ipxe/validator.h>
#include <ipxe/job.h>
#include <ipxe/dhe.h>
#include <ipxe/ecdhe.h>
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
#define ENOTSUP_CURVE __einfo_error ( EINFO_ENOTSUP_CURVE )
#define EINFO_ENOTSUP_CURVE						\
	__einfo_uniqify ( EINFO_ENOTSUP, 0x05,				\
			  "Unsupported elliptic curve" )
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
#define EPROTO_VERSION __einfo_error ( EINFO_EPROTO_VERSION )
#define EINFO_EPROTO_VERSION						\
	__einfo_uniqify ( EINFO_EPROTO, 0x01,				\
			  "Illegal protocol version upgrade" )

/** List of TLS session */
static LIST_HEAD ( tls_sessions );

static void tls_tx_resume_all ( struct tls_session *session );
static struct io_buffer * tls_alloc_iob ( struct tls_connection *tls,
					  size_t len );
static int tls_send_record ( struct tls_connection *tls, unsigned int type,
			     struct io_buffer *iobuf );
static int tls_send_plaintext ( struct tls_connection *tls, unsigned int type,
				const void *data, size_t len );
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
	return ( ( ! is_pending ( &tls->client.negotiation ) ) &&
		 ( ! is_pending ( &tls->server.negotiation ) ) );
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

/******************************************************************************
 *
 * Hybrid MD5+SHA1 hash as used by TLSv1.1 and earlier
 *
 ******************************************************************************
 */

/**
 * Initialise MD5+SHA1 algorithm
 *
 * @v ctx		MD5+SHA1 context
 */
static void md5_sha1_init ( void *ctx ) {
	struct md5_sha1_context *context = ctx;

	digest_init ( &md5_algorithm, context->md5 );
	digest_init ( &sha1_algorithm, context->sha1 );
}

/**
 * Accumulate data with MD5+SHA1 algorithm
 *
 * @v ctx		MD5+SHA1 context
 * @v data		Data
 * @v len		Length of data
 */
static void md5_sha1_update ( void *ctx, const void *data, size_t len ) {
	struct md5_sha1_context *context = ctx;

	digest_update ( &md5_algorithm, context->md5, data, len );
	digest_update ( &sha1_algorithm, context->sha1, data, len );
}

/**
 * Generate MD5+SHA1 digest
 *
 * @v ctx		MD5+SHA1 context
 * @v out		Output buffer
 */
static void md5_sha1_final ( void *ctx, void *out ) {
	struct md5_sha1_context *context = ctx;
	struct md5_sha1_digest *digest = out;

	digest_final ( &md5_algorithm, context->md5, digest->md5 );
	digest_final ( &sha1_algorithm, context->sha1, digest->sha1 );
}

/** Hybrid MD5+SHA1 digest algorithm */
static struct digest_algorithm md5_sha1_algorithm = {
	.name		= "md5+sha1",
	.ctxsize	= sizeof ( struct md5_sha1_context ),
	.blocksize	= 0, /* Not applicable */
	.digestsize	= sizeof ( struct md5_sha1_digest ),
	.init		= md5_sha1_init,
	.update		= md5_sha1_update,
	.final		= md5_sha1_final,
};

/** RSA digestInfo prefix for MD5+SHA1 algorithm */
struct rsa_digestinfo_prefix rsa_md5_sha1_prefix __rsa_digestinfo_prefix = {
	.digest = &md5_sha1_algorithm,
	.data = NULL, /* MD5+SHA1 signatures have no digestInfo */
	.len = 0,
};

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

	/* Free dynamically-allocated resources */
	x509_root_put ( session->root );
	privkey_put ( session->key );
	free ( session->ticket );

	/* Free session */
	free ( session );
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
	free ( tls->new_session_ticket );
	tls_clear_cipher ( tls, &tls->tx.cipherspec.active );
	tls_clear_cipher ( tls, &tls->tx.cipherspec.pending );
	tls_clear_cipher ( tls, &tls->rx.cipherspec.active );
	tls_clear_cipher ( tls, &tls->rx.cipherspec.pending );
	free ( tls->server.exchange );
	free ( tls->handshake_ctx );
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
	free ( tls );
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

	/* Resume all other connections, in case we were the lead connection */
	tls_tx_resume_all ( tls->session );
}

/******************************************************************************
 *
 * Random number generation
 *
 ******************************************************************************
 */

/**
 * Generate random data
 *
 * @v tls		TLS connection
 * @v data		Buffer to fill
 * @v len		Length of buffer
 * @ret rc		Return status code
 */
static int tls_generate_random ( struct tls_connection *tls,
				 void *data, size_t len ) {
	int rc;

	/* Generate random bits with no additional input and without
	 * prediction resistance
	 */
	if ( ( rc = rbg_generate ( NULL, 0, 0, data, len ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not generate random data: %s\n",
		       tls, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Update HMAC with a list of ( data, len ) pairs
 *
 * @v digest		Hash function to use
 * @v ctx		HMAC context
 * @v args		( data, len ) pairs of data, terminated by NULL
 */
static void tls_hmac_update_va ( struct digest_algorithm *digest,
				 void *ctx, va_list args ) {
	void *data;
	size_t len;

	while ( ( data = va_arg ( args, void * ) ) ) {
		len = va_arg ( args, size_t );
		hmac_update ( digest, ctx, data, len );
	}
}

/**
 * Generate secure pseudo-random data using a single hash function
 *
 * @v tls		TLS connection
 * @v digest		Hash function to use
 * @v secret		Secret
 * @v secret_len	Length of secret
 * @v out		Output buffer
 * @v out_len		Length of output buffer
 * @v seeds		( data, len ) pairs of seed data, terminated by NULL
 */
static void tls_p_hash_va ( struct tls_connection *tls,
			    struct digest_algorithm *digest,
			    const void *secret, size_t secret_len,
			    void *out, size_t out_len,
			    va_list seeds ) {
	uint8_t ctx[ hmac_ctxsize ( digest ) ];
	uint8_t ctx_partial[ sizeof ( ctx ) ];
	uint8_t a[digest->digestsize];
	uint8_t out_tmp[digest->digestsize];
	size_t frag_len = digest->digestsize;
	va_list tmp;

	DBGC2 ( tls, "TLS %p %s secret:\n", tls, digest->name );
	DBGC2_HD ( tls, secret, secret_len );

	/* Calculate A(1) */
	hmac_init ( digest, ctx, secret, secret_len );
	va_copy ( tmp, seeds );
	tls_hmac_update_va ( digest, ctx, tmp );
	va_end ( tmp );
	hmac_final ( digest, ctx, a );
	DBGC2 ( tls, "TLS %p %s A(1):\n", tls, digest->name );
	DBGC2_HD ( tls, &a, sizeof ( a ) );

	/* Generate as much data as required */
	while ( out_len ) {
		/* Calculate output portion */
		hmac_init ( digest, ctx, secret, secret_len );
		hmac_update ( digest, ctx, a, sizeof ( a ) );
		memcpy ( ctx_partial, ctx, sizeof ( ctx_partial ) );
		va_copy ( tmp, seeds );
		tls_hmac_update_va ( digest, ctx, tmp );
		va_end ( tmp );
		hmac_final ( digest, ctx, out_tmp );

		/* Copy output */
		if ( frag_len > out_len )
			frag_len = out_len;
		memcpy ( out, out_tmp, frag_len );
		DBGC2 ( tls, "TLS %p %s output:\n", tls, digest->name );
		DBGC2_HD ( tls, out, frag_len );

		/* Calculate A(i) */
		hmac_final ( digest, ctx_partial, a );
		DBGC2 ( tls, "TLS %p %s A(n):\n", tls, digest->name );
		DBGC2_HD ( tls, &a, sizeof ( a ) );

		out += frag_len;
		out_len -= frag_len;
	}
}

/**
 * Generate secure pseudo-random data
 *
 * @v tls		TLS connection
 * @v secret		Secret
 * @v secret_len	Length of secret
 * @v out		Output buffer
 * @v out_len		Length of output buffer
 * @v ...		( data, len ) pairs of seed data, terminated by NULL
 */
static void tls_prf ( struct tls_connection *tls, const void *secret,
		      size_t secret_len, void *out, size_t out_len, ... ) {
	va_list seeds;
	va_list tmp;
	size_t subsecret_len;
	const void *md5_secret;
	const void *sha1_secret;
	uint8_t buf[out_len];
	unsigned int i;

	va_start ( seeds, out_len );

	if ( tls_version ( tls, TLS_VERSION_TLS_1_2 ) ) {
		/* Use handshake digest PRF for TLSv1.2 and later */
		tls_p_hash_va ( tls, tls->handshake_digest, secret, secret_len,
				out, out_len, seeds );
	} else {
		/* Use combination of P_MD5 and P_SHA-1 for TLSv1.1
		 * and earlier
		 */

		/* Split secret into two, with an overlap of up to one byte */
		subsecret_len = ( ( secret_len + 1 ) / 2 );
		md5_secret = secret;
		sha1_secret = ( secret + secret_len - subsecret_len );

		/* Calculate MD5 portion */
		va_copy ( tmp, seeds );
		tls_p_hash_va ( tls, &md5_algorithm, md5_secret,
				subsecret_len, out, out_len, seeds );
		va_end ( tmp );

		/* Calculate SHA1 portion */
		va_copy ( tmp, seeds );
		tls_p_hash_va ( tls, &sha1_algorithm, sha1_secret,
				subsecret_len, buf, out_len, seeds );
		va_end ( tmp );

		/* XOR the two portions together into the final output buffer */
		for ( i = 0 ; i < out_len ; i++ )
			*( ( uint8_t * ) out + i ) ^= buf[i];
	}

	va_end ( seeds );
}

/**
 * Generate secure pseudo-random data
 *
 * @v secret		Secret
 * @v secret_len	Length of secret
 * @v out		Output buffer
 * @v out_len		Length of output buffer
 * @v label		String literal label
 * @v ...		( data, len ) pairs of seed data
 */
#define tls_prf_label( tls, secret, secret_len, out, out_len, label, ... ) \
	tls_prf ( (tls), (secret), (secret_len), (out), (out_len),	   \
		  label, ( sizeof ( label ) - 1 ), __VA_ARGS__, NULL )

/******************************************************************************
 *
 * Secret management
 *
 ******************************************************************************
 */

/**
 * Generate master secret
 *
 * @v tls		TLS connection
 * @v pre_master_secret	Pre-master secret
 * @v pre_master_secret_len Length of pre-master secret
 *
 * The client and server random values must already be known.
 */
static void tls_generate_master_secret ( struct tls_connection *tls,
					 const void *pre_master_secret,
					 size_t pre_master_secret_len ) {

	DBGC ( tls, "TLS %p pre-master-secret:\n", tls );
	DBGC_HD ( tls, pre_master_secret, pre_master_secret_len );
	DBGC ( tls, "TLS %p client random bytes:\n", tls );
	DBGC_HD ( tls, &tls->client.random, sizeof ( tls->client.random ) );
	DBGC ( tls, "TLS %p server random bytes:\n", tls );
	DBGC_HD ( tls, &tls->server.random, sizeof ( tls->server.random ) );

	tls_prf_label ( tls, pre_master_secret, pre_master_secret_len,
			&tls->master_secret, sizeof ( tls->master_secret ),
			"master secret",
			&tls->client.random, sizeof ( tls->client.random ),
			&tls->server.random, sizeof ( tls->server.random ) );

	DBGC ( tls, "TLS %p generated master secret:\n", tls );
	DBGC_HD ( tls, &tls->master_secret, sizeof ( tls->master_secret ) );
}

/**
 * Generate key material
 *
 * @v tls		TLS connection
 *
 * The master secret must already be known.
 */
static int tls_generate_keys ( struct tls_connection *tls ) {
	struct tls_cipherspec *tx_cipherspec = &tls->tx.cipherspec.pending;
	struct tls_cipherspec *rx_cipherspec = &tls->rx.cipherspec.pending;
	size_t hash_size = tx_cipherspec->suite->mac_len;
	size_t key_size = tx_cipherspec->suite->key_len;
	size_t iv_size = tx_cipherspec->suite->fixed_iv_len;
	size_t total = ( 2 * ( hash_size + key_size + iv_size ) );
	uint8_t key_block[total];
	uint8_t *key;
	int rc;

	/* Generate key block */
	tls_prf_label ( tls, &tls->master_secret, sizeof ( tls->master_secret ),
			key_block, sizeof ( key_block ), "key expansion",
			&tls->server.random, sizeof ( tls->server.random ),
			&tls->client.random, sizeof ( tls->client.random ) );

	/* Split key block into portions */
	key = key_block;

	/* TX MAC secret */
	memcpy ( tx_cipherspec->mac_secret, key, hash_size );
	DBGC ( tls, "TLS %p TX MAC secret:\n", tls );
	DBGC_HD ( tls, key, hash_size );
	key += hash_size;

	/* RX MAC secret */
	memcpy ( rx_cipherspec->mac_secret, key, hash_size );
	DBGC ( tls, "TLS %p RX MAC secret:\n", tls );
	DBGC_HD ( tls, key, hash_size );
	key += hash_size;

	/* TX key */
	if ( ( rc = cipher_setkey ( tx_cipherspec->suite->cipher,
				    tx_cipherspec->cipher_ctx,
				    key, key_size ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not set TX key: %s\n",
		       tls, strerror ( rc ) );
		return rc;
	}
	DBGC ( tls, "TLS %p TX key:\n", tls );
	DBGC_HD ( tls, key, key_size );
	key += key_size;

	/* RX key */
	if ( ( rc = cipher_setkey ( rx_cipherspec->suite->cipher,
				    rx_cipherspec->cipher_ctx,
				    key, key_size ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not set TX key: %s\n",
		       tls, strerror ( rc ) );
		return rc;
	}
	DBGC ( tls, "TLS %p RX key:\n", tls );
	DBGC_HD ( tls, key, key_size );
	key += key_size;

	/* TX initialisation vector */
	memcpy ( tx_cipherspec->fixed_iv, key, iv_size );
	DBGC ( tls, "TLS %p TX IV:\n", tls );
	DBGC_HD ( tls, key, iv_size );
	key += iv_size;

	/* RX initialisation vector */
	memcpy ( rx_cipherspec->fixed_iv, key, iv_size );
	DBGC ( tls, "TLS %p RX IV:\n", tls );
	DBGC_HD ( tls, key, iv_size );
	key += iv_size;

	assert ( ( key_block + total ) == key );

	return 0;
}

/******************************************************************************
 *
 * Handshake verification
 *
 ******************************************************************************
 */

/**
 * Clear handshake digest algorithm
 *
 * @v tls		TLS connection
 */
static void tls_clear_handshake ( struct tls_connection *tls ) {

	/* Select null digest algorithm */
	tls->handshake_digest = &digest_null;

	/* Free any existing context */
	free ( tls->handshake_ctx );
	tls->handshake_ctx = NULL;
}

/**
 * Select handshake digest algorithm
 *
 * @v tls		TLS connection
 * @v digest		Handshake digest algorithm
 * @ret rc		Return status code
 */
static int tls_select_handshake ( struct tls_connection *tls,
				  struct digest_algorithm *digest ) {

	/* Clear existing handshake digest */
	tls_clear_handshake ( tls );

	/* Allocate and initialise context */
	tls->handshake_ctx = malloc ( digest->ctxsize );
	if ( ! tls->handshake_ctx )
		return -ENOMEM;
	tls->handshake_digest = digest;
	digest_init ( digest, tls->handshake_ctx );

	return 0;
}

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
	struct digest_algorithm *digest = tls->handshake_digest;

	digest_update ( digest, tls->handshake_ctx, data, len );
	return 0;
}

/**
 * Calculate handshake verification hash
 *
 * @v tls		TLS connection
 * @v out		Output buffer
 *
 * Calculates the digest over all handshake messages seen so far.
 */
static void tls_verify_handshake ( struct tls_connection *tls, void *out ) {
	struct digest_algorithm *digest = tls->handshake_digest;
	uint8_t ctx[ digest->ctxsize ];

	memcpy ( ctx, tls->handshake_ctx, sizeof ( ctx ) );
	digest_final ( digest, ctx, out );
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
 * Clear cipher suite
 *
 * @v cipherspec	TLS cipher specification
 */
static void tls_clear_cipher ( struct tls_connection *tls __unused,
			       struct tls_cipherspec *cipherspec ) {

	free ( cipherspec->dynamic );
	memset ( cipherspec, 0, sizeof ( *cipherspec ) );
	cipherspec->suite = &tls_cipher_suite_null;
}

/**
 * Set cipher suite
 *
 * @v tls		TLS connection
 * @v cipherspec	TLS cipher specification
 * @v suite		Cipher suite
 * @ret rc		Return status code
 */
static int tls_set_cipher ( struct tls_connection *tls,
			    struct tls_cipherspec *cipherspec,
			    struct tls_cipher_suite *suite ) {
	struct cipher_algorithm *cipher = suite->cipher;
	size_t total;
	void *dynamic;

	/* Clear out old cipher contents, if any */
	tls_clear_cipher ( tls, cipherspec );

	/* Allocate dynamic storage */
	total = ( cipher->ctxsize + suite->mac_len + suite->fixed_iv_len );
	dynamic = zalloc ( total );
	if ( ! dynamic ) {
		DBGC ( tls, "TLS %p could not allocate %zd bytes for crypto "
		       "context\n", tls, total );
		return -ENOMEM_CONTEXT;
	}

	/* Assign storage */
	cipherspec->dynamic = dynamic;
	cipherspec->cipher_ctx = dynamic;	dynamic += cipher->ctxsize;
	cipherspec->mac_secret = dynamic;	dynamic += suite->mac_len;
	cipherspec->fixed_iv = dynamic;		dynamic += suite->fixed_iv_len;
	assert ( ( cipherspec->dynamic + total ) == dynamic );

	/* Store parameters */
	cipherspec->suite = suite;

	return 0;
}

/**
 * Select next cipher suite
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

	/* Set handshake digest algorithm */
	digest = ( tls_version ( tls, TLS_VERSION_TLS_1_2 ) ?
		   suite->handshake : &md5_sha1_algorithm );
	if ( ( rc = tls_select_handshake ( tls, digest ) ) != 0 )
		return rc;

	/* Set ciphers */
	if ( ( rc = tls_set_cipher ( tls, &tls->tx.cipherspec.pending,
				     suite ) ) != 0 )
		return rc;
	if ( ( rc = tls_set_cipher ( tls, &tls->rx.cipherspec.pending,
				     suite ) ) != 0 )
		return rc;

	DBGC ( tls, "TLS %p selected %s-%s-%s-%d-%s\n", tls,
	       suite->exchange->name, suite->pubkey->name,
	       suite->cipher->name, ( suite->key_len * 8 ),
	       suite->digest->name );

	return 0;
}

/**
 * Activate next cipher suite
 *
 * @v tls		TLS connection
 * @v pair		Cipher specification pair
 * @ret rc		Return status code
 */
static int tls_change_cipher ( struct tls_connection *tls,
			       struct tls_cipherspec_pair *pair ) {

	/* Sanity check */
	if ( pair->pending.suite == &tls_cipher_suite_null ) {
		DBGC ( tls, "TLS %p refusing to use null cipher\n", tls );
		return -ENOTSUP_NULL;
	}

	tls_clear_cipher ( tls, &pair->active );
	memswap ( &pair->active, &pair->pending, sizeof ( pair->active ) );
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
 * Find TLS signature algorithm
 *
 * @v code		Signature and hash algorithm identifier
 * @ret pubkey		Public key algorithm, or NULL
 */
static struct pubkey_algorithm *
tls_signature_hash_pubkey ( struct tls_signature_hash_id code ) {
	struct tls_signature_hash_algorithm *sig_hash;

	/* Identify signature and hash algorithm */
	for_each_table_entry ( sig_hash, TLS_SIG_HASH_ALGORITHMS ) {
		if ( sig_hash->code.signature == code.signature )
			return sig_hash->pubkey;
	}

	return NULL;
}

/**
 * Find TLS hash algorithm
 *
 * @v code		Signature and hash algorithm identifier
 * @ret digest		Digest algorithm, or NULL
 */
static struct digest_algorithm *
tls_signature_hash_digest ( struct tls_signature_hash_id code ) {
	struct tls_signature_hash_algorithm *sig_hash;

	/* Identify signature and hash algorithm */
	for_each_table_entry ( sig_hash, TLS_SIG_HASH_ALGORITHMS ) {
		if ( sig_hash->code.hash == code.hash )
			return sig_hash->digest;
	}

	return NULL;
}

/******************************************************************************
 *
 * Ephemeral Elliptic Curve Diffie-Hellman key exchange
 *
 ******************************************************************************
 */

/** Number of supported named curves */
#define TLS_NUM_NAMED_CURVES table_num_entries ( TLS_NAMED_CURVES )

/**
 * Identify named curve
 *
 * @v named_curve	Named curve specification
 * @ret curve		Named curve, or NULL
 */
static struct tls_named_curve *
tls_find_named_curve ( unsigned int named_curve ) {
	struct tls_named_curve *curve;

	/* Identify named curve */
	for_each_table_entry ( curve, TLS_NAMED_CURVES ) {
		if ( curve->code == named_curve )
			return curve;
	}

	return NULL;
}

/******************************************************************************
 *
 * Record handling
 *
 ******************************************************************************
 */

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

	/* (Re)start negotiation */
	tls->tx.pending = TLS_TX_CLIENT_HELLO;
	tls_tx_resume ( tls );
	pending_get ( &tls->client.negotiation );
	pending_get ( &tls->server.negotiation );
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
			struct tls_signature_hash_id
				code[TLS_NUM_SIG_HASH_ALGORITHMS];
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
			uint8_t data[session->ticket_len];
		} __attribute__ (( packed )) data;
	} __attribute__ (( packed )) *session_ticket_ext;
	struct {
		uint16_t type;
		uint16_t len;
		struct {
			uint16_t len;
			uint16_t code[TLS_NUM_NAMED_CURVES];
		} __attribute__ (( packed )) data;
	} __attribute__ (( packed )) *named_curve_ext;
	struct {
		typeof ( *server_name_ext ) server_name;
		typeof ( *max_fragment_length_ext ) max_fragment_length;
		typeof ( *signature_algorithms_ext ) signature_algorithms;
		typeof ( *renegotiation_info_ext ) renegotiation_info;
		typeof ( *session_ticket_ext ) session_ticket;
		typeof ( *named_curve_ext )
			named_curve[TLS_NUM_NAMED_CURVES ? 1 : 0];
	} __attribute__ (( packed )) *extensions;
	struct {
		uint32_t type_length;
		uint16_t version;
		uint8_t random[32];
		uint8_t session_id_len;
		uint8_t session_id[tls->session_id_len];
		uint16_t cipher_suite_len;
		uint16_t cipher_suites[TLS_NUM_CIPHER_SUITES];
		uint8_t compression_methods_len;
		uint8_t compression_methods[1];
		uint16_t extensions_len;
		typeof ( *extensions ) extensions;
	} __attribute__ (( packed )) hello;
	struct tls_cipher_suite *suite;
	struct tls_signature_hash_algorithm *sighash;
	struct tls_named_curve *curve;
	unsigned int i;

	/* Construct record */
	memset ( &hello, 0, sizeof ( hello ) );
	hello.type_length = ( cpu_to_le32 ( TLS_CLIENT_HELLO ) |
			      htonl ( sizeof ( hello ) -
				      sizeof ( hello.type_length ) ) );
	hello.version = htons ( TLS_VERSION_MAX );
	memcpy ( &hello.random, &tls->client.random, sizeof ( hello.random ) );
	hello.session_id_len = tls->session_id_len;
	memcpy ( hello.session_id, tls->session_id,
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
	memcpy ( session_ticket_ext->data.data, session->ticket,
		 sizeof ( session_ticket_ext->data.data ) );

	/* Construct named curves extension, if applicable */
	if ( sizeof ( extensions->named_curve ) ) {
		named_curve_ext = &extensions->named_curve[0];
		named_curve_ext->type = htons ( TLS_NAMED_CURVE );
		named_curve_ext->len
			= htons ( sizeof ( named_curve_ext->data ) );
		named_curve_ext->data.len
			= htons ( sizeof ( named_curve_ext->data.code ) );
		i = 0 ; for_each_table_entry ( curve, TLS_NAMED_CURVES )
			named_curve_ext->data.code[i++] = curve->code;
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
 * Transmit Client Key Exchange record using public key exchange
 *
 * @v tls		TLS connection
 * @ret rc		Return status code
 */
static int tls_send_client_key_exchange_pubkey ( struct tls_connection *tls ) {
	struct tls_cipherspec *cipherspec = &tls->tx.cipherspec.pending;
	struct pubkey_algorithm *pubkey = cipherspec->suite->pubkey;
	size_t max_len = pubkey_max_len ( pubkey, &tls->server.key );
	struct {
		uint16_t version;
		uint8_t random[46];
	} __attribute__ (( packed )) pre_master_secret;
	struct {
		uint32_t type_length;
		uint16_t encrypted_pre_master_secret_len;
		uint8_t encrypted_pre_master_secret[max_len];
	} __attribute__ (( packed )) key_xchg;
	size_t unused;
	int len;
	int rc;

	/* Generate pre-master secret */
	pre_master_secret.version = htons ( TLS_VERSION_MAX );
	if ( ( rc = tls_generate_random ( tls, &pre_master_secret.random,
			  ( sizeof ( pre_master_secret.random ) ) ) ) != 0 ) {
		return rc;
	}

	/* Generate master secret */
	tls_generate_master_secret ( tls, &pre_master_secret,
				     sizeof ( pre_master_secret ) );

	/* Encrypt pre-master secret using server's public key */
	memset ( &key_xchg, 0, sizeof ( key_xchg ) );
	len = pubkey_encrypt ( pubkey, &tls->server.key, &pre_master_secret,
			       sizeof ( pre_master_secret ),
			       key_xchg.encrypted_pre_master_secret );
	if ( len < 0 ) {
		rc = len;
		DBGC ( tls, "TLS %p could not encrypt pre-master secret: %s\n",
		       tls, strerror ( rc ) );
		return rc;
	}
	unused = ( max_len - len );
	key_xchg.type_length =
		( cpu_to_le32 ( TLS_CLIENT_KEY_EXCHANGE ) |
		  htonl ( sizeof ( key_xchg ) -
			  sizeof ( key_xchg.type_length ) - unused ) );
	key_xchg.encrypted_pre_master_secret_len =
		htons ( sizeof ( key_xchg.encrypted_pre_master_secret ) -
			unused );

	return tls_send_handshake ( tls, &key_xchg,
				    ( sizeof ( key_xchg ) - unused ) );
}

/** Public key exchange algorithm */
struct tls_key_exchange_algorithm tls_pubkey_exchange_algorithm = {
	.name = "pubkey",
	.exchange = tls_send_client_key_exchange_pubkey,
};

/**
 * Verify Diffie-Hellman parameter signature
 *
 * @v tls		TLS connection
 * @v param_len		Diffie-Hellman parameter length
 * @ret rc		Return status code
 */
static int tls_verify_dh_params ( struct tls_connection *tls,
				  size_t param_len ) {
	struct tls_cipherspec *cipherspec = &tls->tx.cipherspec.pending;
	struct pubkey_algorithm *pubkey;
	struct digest_algorithm *digest;
	int use_sig_hash = tls_version ( tls, TLS_VERSION_TLS_1_2 );
	const struct {
		struct tls_signature_hash_id sig_hash[use_sig_hash];
		uint16_t signature_len;
		uint8_t signature[0];
	} __attribute__ (( packed )) *sig;
	const void *data;
	size_t remaining;
	int rc;

	/* Signature follows parameters */
	assert ( param_len <= tls->server.exchange_len );
	data = ( tls->server.exchange + param_len );
	remaining = ( tls->server.exchange_len - param_len );

	/* Parse signature from ServerKeyExchange */
	sig = data;
	if ( ( sizeof ( *sig ) > remaining ) ||
	     ( ntohs ( sig->signature_len ) > ( remaining -
						sizeof ( *sig ) ) ) ) {
		DBGC ( tls, "TLS %p received underlength ServerKeyExchange\n",
		       tls );
		DBGC_HDA ( tls, 0, tls->server.exchange,
			   tls->server.exchange_len );
		return -EINVAL_KEY_EXCHANGE;
	}

	/* Identify signature and hash algorithm */
	if ( use_sig_hash ) {
		pubkey = tls_signature_hash_pubkey ( sig->sig_hash[0] );
		digest = tls_signature_hash_digest ( sig->sig_hash[0] );
		if ( ( ! pubkey ) || ( ! digest ) ) {
			DBGC ( tls, "TLS %p ServerKeyExchange unsupported "
			       "signature and hash algorithm\n", tls );
			return -ENOTSUP_SIG_HASH;
		}
		if ( pubkey != cipherspec->suite->pubkey ) {
			DBGC ( tls, "TLS %p ServerKeyExchange incorrect "
			       "signature algorithm %s (expected %s)\n", tls,
			       pubkey->name, cipherspec->suite->pubkey->name );
			return -EPERM_KEY_EXCHANGE;
		}
	} else {
		pubkey = cipherspec->suite->pubkey;
		digest = &md5_sha1_algorithm;
	}

	/* Verify signature */
	{
		const void *signature = sig->signature;
		size_t signature_len = ntohs ( sig->signature_len );
		uint8_t ctx[digest->ctxsize];
		uint8_t hash[digest->digestsize];

		/* Calculate digest */
		digest_init ( digest, ctx );
		digest_update ( digest, ctx, &tls->client.random,
				sizeof ( tls->client.random ) );
		digest_update ( digest, ctx, tls->server.random,
				sizeof ( tls->server.random ) );
		digest_update ( digest, ctx, tls->server.exchange, param_len );
		digest_final ( digest, ctx, hash );

		/* Verify signature */
		if ( ( rc = pubkey_verify ( pubkey, &tls->server.key,
					    digest, hash, signature,
					    signature_len ) ) != 0 ) {
			DBGC ( tls, "TLS %p ServerKeyExchange failed "
			       "verification\n", tls );
			DBGC_HDA ( tls, 0, tls->server.exchange,
				   tls->server.exchange_len );
			return -EPERM_KEY_EXCHANGE;
		}
	}

	return 0;
}

/**
 * Transmit Client Key Exchange record using DHE key exchange
 *
 * @v tls		TLS connection
 * @ret rc		Return status code
 */
static int tls_send_client_key_exchange_dhe ( struct tls_connection *tls ) {
	uint8_t private[ sizeof ( tls->client.random.random ) ];
	const struct {
		uint16_t len;
		uint8_t data[0];
	} __attribute__ (( packed )) *dh_val[3];
	const void *data;
	size_t remaining;
	size_t frag_len;
	size_t param_len;
	unsigned int i;
	int rc;

	/* Parse ServerKeyExchange */
	data = tls->server.exchange;
	remaining = tls->server.exchange_len;
	for ( i = 0 ; i < ( sizeof ( dh_val ) / sizeof ( dh_val[0] ) ) ; i++ ){
		dh_val[i] = data;
		if ( ( sizeof ( *dh_val[i] ) > remaining ) ||
		     ( ntohs ( dh_val[i]->len ) > ( remaining -
						    sizeof ( *dh_val[i] ) ) )){
			DBGC ( tls, "TLS %p received underlength "
			       "ServerKeyExchange\n", tls );
			DBGC_HDA ( tls, 0, tls->server.exchange,
				   tls->server.exchange_len );
			rc = -EINVAL_KEY_EXCHANGE;
			goto err_header;
		}
		frag_len = ( sizeof ( *dh_val[i] ) + ntohs ( dh_val[i]->len ));
		data += frag_len;
		remaining -= frag_len;
	}
	param_len = ( tls->server.exchange_len - remaining );

	/* Verify parameter signature */
	if ( ( rc = tls_verify_dh_params ( tls, param_len ) ) != 0 )
		goto err_verify;

	/* Generate Diffie-Hellman private key */
	if ( ( rc = tls_generate_random ( tls, private,
					  sizeof ( private ) ) ) != 0 ) {
		goto err_random;
	}

	/* Construct pre-master secret and ClientKeyExchange record */
	{
		typeof ( dh_val[0] ) dh_p = dh_val[0];
		typeof ( dh_val[1] ) dh_g = dh_val[1];
		typeof ( dh_val[2] ) dh_ys = dh_val[2];
		size_t len = ntohs ( dh_p->len );
		struct {
			uint32_t type_length;
			uint16_t dh_xs_len;
			uint8_t dh_xs[len];
		} __attribute__ (( packed )) *key_xchg;
		struct {
			uint8_t pre_master_secret[len];
			typeof ( *key_xchg ) key_xchg;
		} *dynamic;
		uint8_t *pre_master_secret;

		/* Allocate space */
		dynamic = malloc ( sizeof ( *dynamic ) );
		if ( ! dynamic ) {
			rc = -ENOMEM;
			goto err_alloc;
		}
		pre_master_secret = dynamic->pre_master_secret;
		key_xchg = &dynamic->key_xchg;
		key_xchg->type_length =
			( cpu_to_le32 ( TLS_CLIENT_KEY_EXCHANGE ) |
			  htonl ( sizeof ( *key_xchg ) -
				  sizeof ( key_xchg->type_length ) ) );
		key_xchg->dh_xs_len = htons ( len );

		/* Calculate pre-master secret and client public value */
		if ( ( rc = dhe_key ( dh_p->data, len,
				      dh_g->data, ntohs ( dh_g->len ),
				      dh_ys->data, ntohs ( dh_ys->len ),
				      private, sizeof ( private ),
				      key_xchg->dh_xs,
				      pre_master_secret ) ) != 0 ) {
			DBGC ( tls, "TLS %p could not calculate DHE key: %s\n",
			       tls, strerror ( rc ) );
			goto err_dhe_key;
		}

		/* Strip leading zeroes from pre-master secret */
		while ( len && ( ! *pre_master_secret ) ) {
			pre_master_secret++;
			len--;
		}

		/* Generate master secret */
		tls_generate_master_secret ( tls, pre_master_secret, len );

		/* Transmit Client Key Exchange record */
		if ( ( rc = tls_send_handshake ( tls, key_xchg,
						 sizeof ( *key_xchg ) ) ) !=0){
			goto err_send_handshake;
		}

	err_send_handshake:
	err_dhe_key:
		free ( dynamic );
	}
 err_alloc:
 err_random:
 err_verify:
 err_header:
	return rc;
}

/** Ephemeral Diffie-Hellman key exchange algorithm */
struct tls_key_exchange_algorithm tls_dhe_exchange_algorithm = {
	.name = "dhe",
	.exchange = tls_send_client_key_exchange_dhe,
};

/**
 * Transmit Client Key Exchange record using ECDHE key exchange
 *
 * @v tls		TLS connection
 * @ret rc		Return status code
 */
static int tls_send_client_key_exchange_ecdhe ( struct tls_connection *tls ) {
	struct tls_named_curve *curve;
	const struct {
		uint8_t curve_type;
		uint16_t named_curve;
		uint8_t public_len;
		uint8_t public[0];
	} __attribute__ (( packed )) *ecdh;
	size_t param_len;
	size_t pointsize;
	size_t keysize;
	size_t offset;
	int rc;

	/* Parse ServerKeyExchange record */
	ecdh = tls->server.exchange;
	if ( ( sizeof ( *ecdh ) > tls->server.exchange_len ) ||
	     ( ecdh->public_len > ( tls->server.exchange_len -
				    sizeof ( *ecdh ) ) ) ) {
		DBGC ( tls, "TLS %p received underlength ServerKeyExchange\n",
		       tls );
		DBGC_HDA ( tls, 0, tls->server.exchange,
			   tls->server.exchange_len );
		return -EINVAL_KEY_EXCHANGE;
	}
	param_len = ( sizeof ( *ecdh ) + ecdh->public_len );

	/* Verify parameter signature */
	if ( ( rc = tls_verify_dh_params ( tls, param_len ) ) != 0 )
		return rc;

	/* Identify named curve */
	if ( ecdh->curve_type != TLS_NAMED_CURVE_TYPE ) {
		DBGC ( tls, "TLS %p unsupported curve type %d\n",
		       tls, ecdh->curve_type );
		DBGC_HDA ( tls, 0, tls->server.exchange,
			   tls->server.exchange_len );
		return -ENOTSUP_CURVE;
	}
	curve = tls_find_named_curve ( ecdh->named_curve );
	if ( ! curve ) {
		DBGC ( tls, "TLS %p unsupported named curve %d\n",
		       tls, ntohs ( ecdh->named_curve ) );
		DBGC_HDA ( tls, 0, tls->server.exchange,
			   tls->server.exchange_len );
		return -ENOTSUP_CURVE;
	}
	DBGC ( tls, "TLS %p using named curve %s\n", tls, curve->curve->name );
	pointsize = curve->curve->pointsize;
	keysize = curve->curve->keysize;
	offset = ( curve->format ? 1 : 0 );

	/* Check key length */
	if ( ecdh->public_len != ( offset + pointsize ) ) {
		DBGC ( tls, "TLS %p invalid %s key\n",
		       tls, curve->curve->name );
		DBGC_HDA ( tls, 0, tls->server.exchange,
			   tls->server.exchange_len );
		return -EINVAL_KEY_EXCHANGE;
	}

	/* Check curve point format byte (if present) */
	if ( curve->format && ( ecdh->public[0] != curve->format ) ) {
		DBGC ( tls, "TLS %p invalid %s curve point format\n",
		       tls, curve->curve->name );
		DBGC_HDA ( tls, 0, tls->server.exchange,
			   tls->server.exchange_len );
		return -EINVAL_KEY_EXCHANGE;
	}

	/* Construct pre-master secret and ClientKeyExchange record */
	{
		uint8_t private[keysize];
		uint8_t pre_master_secret[pointsize];
		struct {
			uint32_t type_length;
			uint8_t public_len;
			uint8_t public[ecdh->public_len];
		} __attribute__ (( packed )) key_xchg;

		/* Generate ephemeral private key */
		if ( ( rc = tls_generate_random ( tls, private,
						  sizeof ( private ) ) ) != 0){
			return rc;
		}

		/* Exchange keys */
		if ( ( rc = ecdhe_key ( curve->curve, ( ecdh->public + offset ),
					private, ( key_xchg.public + offset ),
					pre_master_secret ) ) != 0 ) {
			DBGC ( tls, "TLS %p could not exchange ECDHE key: %s\n",
			       tls, strerror ( rc ) );
			return rc;
		}

		/* Generate master secret */
		tls_generate_master_secret ( tls, pre_master_secret,
					     curve->pre_master_secret_len );

		/* Generate Client Key Exchange record */
		key_xchg.type_length =
			( cpu_to_le32 ( TLS_CLIENT_KEY_EXCHANGE ) |
			  htonl ( sizeof ( key_xchg ) -
				  sizeof ( key_xchg.type_length ) ) );
		key_xchg.public_len = sizeof ( key_xchg.public );
		if ( curve->format )
			key_xchg.public[0] = curve->format;

		/* Transmit Client Key Exchange record */
		if ( ( rc = tls_send_handshake ( tls, &key_xchg,
						 sizeof ( key_xchg ) ) ) !=0){
			return rc;
		}
	}

	return 0;
}

/** Ephemeral Elliptic Curve Diffie-Hellman key exchange algorithm */
struct tls_key_exchange_algorithm tls_ecdhe_exchange_algorithm = {
	.name = "ecdhe",
	.exchange = tls_send_client_key_exchange_ecdhe,
};

/**
 * Transmit Client Key Exchange record
 *
 * @v tls		TLS connection
 * @ret rc		Return status code
 */
static int tls_send_client_key_exchange ( struct tls_connection *tls ) {
	struct tls_cipherspec *cipherspec = &tls->tx.cipherspec.pending;
	struct tls_cipher_suite *suite = cipherspec->suite;
	int rc;

	/* Transmit Client Key Exchange record via key exchange algorithm */
	if ( ( rc = suite->exchange->exchange ( tls ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not exchange keys: %s\n",
		       tls, strerror ( rc ) );
		return rc;
	}

	/* Generate keys from master secret */
	if ( ( rc = tls_generate_keys ( tls ) ) != 0 ) {
		DBGC ( tls, "TLS %p could not generate keys: %s\n",
		       tls, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Transmit Certificate Verify record
 *
 * @v tls		TLS connection
 * @ret rc		Return status code
 */
static int tls_send_certificate_verify ( struct tls_connection *tls ) {
	struct digest_algorithm *digest = tls->handshake_digest;
	struct x509_certificate *cert = x509_first ( tls->client.chain );
	struct pubkey_algorithm *pubkey = cert->signature_algorithm->pubkey;
	struct asn1_cursor *key = privkey_cursor ( tls->client.key );
	uint8_t digest_out[ digest->digestsize ];
	struct tls_signature_hash_algorithm *sig_hash = NULL;
	int rc;

	/* Generate digest to be signed */
	tls_verify_handshake ( tls, digest_out );

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

	/* Generate and transmit record */
	{
		size_t max_len = pubkey_max_len ( pubkey, key );
		int use_sig_hash = ( ( sig_hash == NULL ) ? 0 : 1 );
		struct {
			uint32_t type_length;
			struct tls_signature_hash_id sig_hash[use_sig_hash];
			uint16_t signature_len;
			uint8_t signature[max_len];
		} __attribute__ (( packed )) certificate_verify;
		size_t unused;
		int len;

		/* Sign digest */
		len = pubkey_sign ( pubkey, key, digest, digest_out,
				    certificate_verify.signature );
		if ( len < 0 ) {
			rc = len;
			DBGC ( tls, "TLS %p could not sign %s digest using %s "
			       "client private key: %s\n", tls, digest->name,
			       pubkey->name, strerror ( rc ) );
			goto err_pubkey_sign;
		}
		unused = ( max_len - len );

		/* Construct Certificate Verify record */
		certificate_verify.type_length =
			( cpu_to_le32 ( TLS_CERTIFICATE_VERIFY ) |
			  htonl ( sizeof ( certificate_verify ) -
				  sizeof ( certificate_verify.type_length ) -
				  unused ) );
		if ( use_sig_hash ) {
			memcpy ( &certificate_verify.sig_hash[0],
				 &sig_hash->code,
				 sizeof ( certificate_verify.sig_hash[0] ) );
		}
		certificate_verify.signature_len =
			htons ( sizeof ( certificate_verify.signature ) -
				unused );

		/* Transmit record */
		rc = tls_send_handshake ( tls, &certificate_verify,
				   ( sizeof ( certificate_verify ) - unused ) );
	}

 err_pubkey_sign:
 err_sig_hash:
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
	struct digest_algorithm *digest = tls->handshake_digest;
	struct {
		uint32_t type_length;
		uint8_t verify_data[ sizeof ( tls->verify.client ) ];
	} __attribute__ (( packed )) finished;
	uint8_t digest_out[ digest->digestsize ];
	int rc;

	/* Construct client verification data */
	tls_verify_handshake ( tls, digest_out );
	tls_prf_label ( tls, &tls->master_secret, sizeof ( tls->master_secret ),
			tls->verify.client, sizeof ( tls->verify.client ),
			"client finished", digest_out, sizeof ( digest_out ) );

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

	return 0;
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
	if ( ( rc = tls_change_cipher ( tls, &tls->rx.cipherspec ) ) != 0 ) {
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
		DBGC ( tls, "TLS %p received warning alert %d\n",
		       tls, alert->description );
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
	if ( ! tls->secure_renegotiation ) {
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
	const struct {
		uint16_t version;
		uint8_t random[32];
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

	/* Add preceding Client Hello to handshake digest */
	if ( ( rc = tls_client_hello ( tls, tls_add_handshake ) ) != 0 )
		return rc;

	/* Copy out server random bytes */
	memcpy ( &tls->server.random, &hello_a->random,
		 sizeof ( tls->server.random ) );

	/* Check session ID */
	if ( hello_a->session_id_len &&
	     ( hello_a->session_id_len == tls->session_id_len ) &&
	     ( memcmp ( session_id, tls->session_id,
			tls->session_id_len ) == 0 ) ) {

		/* Session ID match: reuse master secret */
		DBGC ( tls, "TLS %p resuming session ID:\n", tls );
		DBGC_HDA ( tls, 0, tls->session_id, tls->session_id_len );
		if ( ( rc = tls_generate_keys ( tls ) ) != 0 )
			return rc;

	} else {

		/* Record new session ID, if present */
		if ( hello_a->session_id_len &&
		     ( hello_a->session_id_len <= sizeof ( tls->session_id ))){
			tls->session_id_len = hello_a->session_id_len;
			memcpy ( tls->session_id, session_id,
				 tls->session_id_len );
			DBGC ( tls, "TLS %p new session ID:\n", tls );
			DBGC_HDA ( tls, 0, tls->session_id,
				   tls->session_id_len );
		}
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
	free ( tls->new_session_ticket );
	tls->new_session_ticket = NULL;
	tls->new_session_ticket_len = 0;

	/* Record ticket */
	tls->new_session_ticket = malloc ( ticket_len );
	if ( ! tls->new_session_ticket )
		return -ENOMEM;
	memcpy ( tls->new_session_ticket, new_session_ticket->ticket,
		 ticket_len );
	tls->new_session_ticket_len = ticket_len;
	DBGC ( tls, "TLS %p new session ticket:\n", tls );
	DBGC_HDA ( tls, 0, tls->new_session_ticket,
		   tls->new_session_ticket_len );

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
	size_t remaining = len;
	int rc;

	/* Free any existing certificate chain */
	memset ( &tls->server.key, 0, sizeof ( tls->server.key ) );
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
		struct x509_certificate *cert;

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
	memset ( &tls->server.key, 0, sizeof ( tls->server.key ) );
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

	/* Free any existing server key exchange record */
	free ( tls->server.exchange );
	tls->server.exchange_len = 0;

	/* Allocate copy of server key exchange record */
	tls->server.exchange = malloc ( len );
	if ( ! tls->server.exchange )
		return -ENOMEM;

	/* Store copy of server key exchange record for later
	 * processing.  We cannot verify the signature at this point
	 * since the certificate validation will not yet have
	 * completed.
	 */
	memcpy ( tls->server.exchange, data, len );
	tls->server.exchange_len = len;

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
	struct tls_session *session = tls->session;
	struct digest_algorithm *digest = tls->handshake_digest;
	const struct {
		uint8_t verify_data[ sizeof ( tls->verify.server ) ];
		char next[0];
	} __attribute__ (( packed )) *finished = data;
	uint8_t digest_out[ digest->digestsize ];

	/* Sanity check */
	if ( sizeof ( *finished ) != len ) {
		DBGC ( tls, "TLS %p received overlength Finished\n", tls );
		DBGC_HD ( tls, data, len );
		return -EINVAL_FINISHED;
	}

	/* Verify data */
	tls_verify_handshake ( tls, digest_out );
	tls_prf_label ( tls, &tls->master_secret, sizeof ( tls->master_secret ),
			tls->verify.server, sizeof ( tls->verify.server ),
			"server finished", digest_out, sizeof ( digest_out ) );
	if ( memcmp ( tls->verify.server, finished->verify_data,
		      sizeof ( tls->verify.server ) ) != 0 ) {
		DBGC ( tls, "TLS %p verification failed\n", tls );
		return -EPERM_VERIFY;
	}

	/* Mark server as finished */
	pending_put ( &tls->server.negotiation );

	/* If we are resuming a session (i.e. if the server Finished
	 * arrives before the client Finished is sent), then schedule
	 * transmission of Change Cipher and Finished.
	 */
	if ( is_pending ( &tls->client.negotiation ) ) {
		tls->tx.pending |= ( TLS_TX_CHANGE_CIPHER | TLS_TX_FINISHED );
		tls_tx_resume ( tls );
	}

	/* Record session ID, ticket, and master secret, if applicable */
	if ( tls->session_id_len || tls->new_session_ticket_len ) {
		memcpy ( session->master_secret, tls->master_secret,
			 sizeof ( session->master_secret ) );
	}
	if ( tls->session_id_len ) {
		session->id_len = tls->session_id_len;
		memcpy ( session->id, tls->session_id, sizeof ( session->id ) );
	}
	if ( tls->new_session_ticket_len ) {
		free ( session->ticket );
		session->ticket = tls->new_session_ticket;
		session->ticket_len = tls->new_session_ticket_len;
		tls->new_session_ticket = NULL;
		tls->new_session_ticket_len = 0;
	}

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

		/* Construct and set initialisation vector */
		memcpy ( iv.fixed, cipherspec->fixed_iv, sizeof ( iv.fixed ) );
		if ( ( rc = tls_generate_random ( tls, iv.rec,
						  sizeof ( iv.rec ) ) ) != 0 ) {
			goto err_random;
		}
		cipher_setiv ( cipher, cipherspec->cipher_ctx, &iv,
			       sizeof ( iv ) );

		/* Construct and process authentication data */
		authhdr.seq = cpu_to_be64 ( tls->tx.seq );
		authhdr.header.type = type;
		authhdr.header.version = htons ( tls->version );
		authhdr.header.length = htons ( record_len );
		if ( suite->mac_len ) {
			tls_hmac ( cipherspec, &authhdr, plaintext, record_len,
				   mac );
		}
		if ( is_auth_cipher ( cipher ) ) {
			cipher_encrypt ( cipher, cipherspec->cipher_ctx,
					 &authhdr, NULL, sizeof ( authhdr ) );
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
		cipher_encrypt ( cipher, cipherspec->cipher_ctx, encrypt,
				 ciphertext, encrypt_len );
		cipher_auth ( cipher, cipherspec->cipher_ctx,
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
 err_random:
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
	cipher_setiv ( cipher, cipherspec->cipher_ctx, &iv, sizeof ( iv ) );

	/* Process authentication data, if applicable */
	if ( is_auth_cipher ( cipher ) ) {
		cipher_decrypt ( cipher, cipherspec->cipher_ctx, &authhdr,
				 NULL, sizeof ( authhdr ) );
	}

	/* Decrypt the received data */
	check_len = 0;
	list_for_each_entry ( iobuf, &tls->rx.data, list ) {
		cipher_decrypt ( cipher, cipherspec->cipher_ctx,
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
	cipher_auth ( cipher, cipherspec->cipher_ctx, verify_auth );

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
	INTF_OP ( intf_close, struct tls_connection *, tls_close ),
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
	while ( remaining ) {

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
	}

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
				tls_close ( tls, rc );
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
	struct tls_session *session = tls->session;
	struct x509_certificate *cert;

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

	/* Extract first certificate */
	cert = x509_first ( tls->server.chain );
	assert ( cert != NULL );

	/* Verify server name */
	if ( ( rc = x509_check_name ( cert, session->name ) ) != 0 ) {
		DBGC ( tls, "TLS %p server certificate does not match %s: %s\n",
		       tls, session->name, strerror ( rc ) );
		goto err;
	}

	/* Extract the now trusted server public key */
	memcpy ( &tls->server.key, &cert->subject.public_key.raw,
		 sizeof ( tls->server.key ) );

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
	tls_close ( tls, rc );
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
		/* Record or generate session ID and associated master secret */
		if ( session->id_len ) {
			/* Attempt to resume an existing session */
			memcpy ( tls->session_id, session->id,
				 sizeof ( tls->session_id ) );
			tls->session_id_len = session->id_len;
			memcpy ( tls->master_secret, session->master_secret,
				 sizeof ( tls->master_secret ) );
		} else {
			/* No existing session: use a random session ID */
			assert ( sizeof ( tls->session_id ) ==
				 sizeof ( tls->client.random ) );
			memcpy ( tls->session_id, &tls->client.random,
				 sizeof ( tls->session_id ) );
			tls->session_id_len = sizeof ( tls->session_id );
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
			DBGC ( tls, "TLS %p cold not send Certificate: %s\n",
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
		if ( ( rc = tls_change_cipher ( tls,
						&tls->tx.cipherspec ) ) != 0 ){
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
	tls_close ( tls, rc );
}

/** TLS TX process descriptor */
static struct process_descriptor tls_process_desc =
	PROC_DESC_ONCE ( struct tls_connection, tx.process, tls_tx_step );

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

	/* Record session */
	tls->session = session;

	DBGC ( tls, "TLS %p created session %s\n", tls, name );
	return 0;

	ref_put ( &session->refcnt );
 err_alloc:
	return rc;
}

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
	tls_clear_cipher ( tls, &tls->tx.cipherspec.active );
	tls_clear_cipher ( tls, &tls->tx.cipherspec.pending );
	tls_clear_cipher ( tls, &tls->rx.cipherspec.active );
	tls_clear_cipher ( tls, &tls->rx.cipherspec.pending );
	tls_clear_handshake ( tls );
	tls->client.random.gmt_unix_time = time ( NULL );
	iob_populate ( &tls->rx.iobuf, &tls->rx.header, 0,
		       sizeof ( tls->rx.header ) );
	INIT_LIST_HEAD ( &tls->rx.data );
	if ( ( rc = tls_generate_random ( tls, &tls->client.random.random,
			  ( sizeof ( tls->client.random.random ) ) ) ) != 0 ) {
		goto err_random;
	}
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
 err_random:
	ref_put ( &tls->refcnt );
 err_alloc:
	return rc;
}

/* Drag in objects via add_tls() */
REQUIRING_SYMBOL ( add_tls );

/* Drag in crypto configuration */
REQUIRE_OBJECT ( config_crypto );
