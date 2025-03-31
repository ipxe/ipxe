#ifndef _IPXE_TLS_H
#define _IPXE_TLS_H

/**
 * @file
 *
 * Transport Layer Security Protocol
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/refcnt.h>
#include <ipxe/interface.h>
#include <ipxe/process.h>
#include <ipxe/crypto.h>
#include <ipxe/md5.h>
#include <ipxe/sha1.h>
#include <ipxe/x509.h>
#include <ipxe/privkey.h>
#include <ipxe/pending.h>
#include <ipxe/iobuf.h>
#include <ipxe/tables.h>

struct tls_connection;

/** A TLS header */
struct tls_header {
	/** Content type
	 *
	 * This is a TLS_TYPE_XXX constant
	 */
	uint8_t type;
	/** Protocol version
	 *
	 * This is a TLS_VERSION_XXX constant
	 */
	uint16_t version;
	/** Length of payload */
	uint16_t length;
} __attribute__ (( packed ));

/** TLS version 1.1 */
#define TLS_VERSION_TLS_1_1 0x0302

/** TLS version 1.2 */
#define TLS_VERSION_TLS_1_2 0x0303

/** Maximum supported TLS version */
#define TLS_VERSION_MAX TLS_VERSION_TLS_1_2

/** Change cipher content type */
#define TLS_TYPE_CHANGE_CIPHER 20

/** Change cipher spec magic byte */
#define TLS_CHANGE_CIPHER_SPEC 1

/** Alert content type */
#define TLS_TYPE_ALERT 21

/** Handshake content type */
#define TLS_TYPE_HANDSHAKE 22

/** Application data content type */
#define TLS_TYPE_DATA 23

/* Handshake message types */
#define TLS_HELLO_REQUEST 0
#define TLS_CLIENT_HELLO 1
#define TLS_SERVER_HELLO 2
#define TLS_NEW_SESSION_TICKET 4
#define TLS_CERTIFICATE 11
#define TLS_SERVER_KEY_EXCHANGE 12
#define TLS_CERTIFICATE_REQUEST 13
#define TLS_SERVER_HELLO_DONE 14
#define TLS_CERTIFICATE_VERIFY 15
#define TLS_CLIENT_KEY_EXCHANGE 16
#define TLS_FINISHED 20

/* TLS alert levels */
#define TLS_ALERT_WARNING 1
#define TLS_ALERT_FATAL 2

/* TLS cipher specifications */
#define TLS_RSA_WITH_NULL_MD5 0x0001
#define TLS_RSA_WITH_NULL_SHA 0x0002
#define TLS_RSA_WITH_AES_128_CBC_SHA 0x002f
#define TLS_DHE_RSA_WITH_AES_128_CBC_SHA 0x0033
#define TLS_RSA_WITH_AES_256_CBC_SHA 0x0035
#define TLS_DHE_RSA_WITH_AES_256_CBC_SHA 0x0039
#define TLS_RSA_WITH_AES_128_CBC_SHA256 0x003c
#define TLS_RSA_WITH_AES_256_CBC_SHA256 0x003d
#define TLS_DHE_RSA_WITH_AES_128_CBC_SHA256 0x0067
#define TLS_DHE_RSA_WITH_AES_256_CBC_SHA256 0x006b
#define TLS_RSA_WITH_AES_128_GCM_SHA256 0x009c
#define TLS_RSA_WITH_AES_256_GCM_SHA384 0x009d
#define TLS_DHE_RSA_WITH_AES_128_GCM_SHA256 0x009e
#define TLS_DHE_RSA_WITH_AES_256_GCM_SHA384 0x009f
#define TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA 0xc013
#define TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA 0xc014
#define TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256 0xc027
#define TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384 0xc028
#define TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 0xc02f
#define TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384 0xc030

/* TLS hash algorithm identifiers */
#define TLS_MD5_ALGORITHM 1
#define TLS_SHA1_ALGORITHM 2
#define TLS_SHA224_ALGORITHM 3
#define TLS_SHA256_ALGORITHM 4
#define TLS_SHA384_ALGORITHM 5
#define TLS_SHA512_ALGORITHM 6

/* TLS signature algorithm identifiers */
#define TLS_RSA_ALGORITHM 1

/* TLS server name extension */
#define TLS_SERVER_NAME 0
#define TLS_SERVER_NAME_HOST_NAME 0

/* TLS maximum fragment length extension */
#define TLS_MAX_FRAGMENT_LENGTH 1
#define TLS_MAX_FRAGMENT_LENGTH_512 1
#define TLS_MAX_FRAGMENT_LENGTH_1024 2
#define TLS_MAX_FRAGMENT_LENGTH_2048 3
#define TLS_MAX_FRAGMENT_LENGTH_4096 4

/* TLS named curve extension */
#define TLS_NAMED_CURVE 10
#define TLS_NAMED_CURVE_SECP256R1 23
#define TLS_NAMED_CURVE_SECP384R1 24
#define TLS_NAMED_CURVE_X25519 29

/* TLS signature algorithms extension */
#define TLS_SIGNATURE_ALGORITHMS 13

/* TLS session ticket extension */
#define TLS_SESSION_TICKET 35

/* TLS renegotiation information extension */
#define TLS_RENEGOTIATION_INFO 0xff01

/** TLS authentication header */
struct tls_auth_header {
	/** Sequence number */
	uint64_t seq;
	/** TLS header */
	struct tls_header header;
} __attribute__ (( packed ));

/** TLS verification data */
struct tls_verify_data {
	/** Client verification data */
	uint8_t client[12];
	/** Server verification data */
	uint8_t server[12];
} __attribute__ (( packed ));

/** TLS RX state machine state */
enum tls_rx_state {
	TLS_RX_HEADER = 0,
	TLS_RX_DATA,
};

/** TLS TX pending flags */
enum tls_tx_pending {
	TLS_TX_CLIENT_HELLO = 0x0001,
	TLS_TX_CERTIFICATE = 0x0002,
	TLS_TX_CLIENT_KEY_EXCHANGE = 0x0004,
	TLS_TX_CERTIFICATE_VERIFY = 0x0008,
	TLS_TX_CHANGE_CIPHER = 0x0010,
	TLS_TX_FINISHED = 0x0020,
};

/** A TLS key exchange algorithm */
struct tls_key_exchange_algorithm {
	/** Algorithm name */
	const char *name;
	/**
	 * Transmit Client Key Exchange record
	 *
	 * @v tls		TLS connection
	 * @ret rc		Return status code
	 */
	int ( * exchange ) ( struct tls_connection *tls );
};

/** A TLS cipher suite */
struct tls_cipher_suite {
	/** Key exchange algorithm */
	struct tls_key_exchange_algorithm *exchange;
	/** Public-key encryption algorithm */
	struct pubkey_algorithm *pubkey;
	/** Bulk encryption cipher algorithm */
	struct cipher_algorithm *cipher;
	/** MAC digest algorithm */
	struct digest_algorithm *digest;
	/** Handshake digest algorithm (for TLSv1.2 and above) */
	struct digest_algorithm *handshake;
	/** Numeric code (in network-endian order) */
	uint16_t code;
	/** Key length */
	uint8_t key_len;
	/** Fixed initialisation vector length */
	uint8_t fixed_iv_len;
	/** Record initialisation vector length */
	uint8_t record_iv_len;
	/** MAC length */
	uint8_t mac_len;
};

/** TLS cipher suite table */
#define TLS_CIPHER_SUITES						\
	__table ( struct tls_cipher_suite, "tls_cipher_suites" )

/** Declare a TLS cipher suite */
#define __tls_cipher_suite( pref )					\
	__table_entry ( TLS_CIPHER_SUITES, pref )

/** TLS named curved type */
#define TLS_NAMED_CURVE_TYPE 3

/** TLS uncompressed curve point format */
#define TLS_POINT_FORMAT_UNCOMPRESSED 4

/** A TLS named curve */
struct tls_named_curve {
	/** Elliptic curve */
	struct elliptic_curve *curve;
	/** Numeric code (in network-endian order) */
	uint16_t code;
	/** Curve point format byte (if any) */
	uint8_t format;
	/** Pre-master secret length */
	uint8_t pre_master_secret_len;
};

/** TLS named curve table */
#define TLS_NAMED_CURVES						\
	__table ( struct tls_named_curve, "tls_named_curves" )

/** Declare a TLS named curve */
#define __tls_named_curve( pref )					\
	__table_entry ( TLS_NAMED_CURVES, pref )

/** A TLS cipher specification */
struct tls_cipherspec {
	/** Cipher suite */
	struct tls_cipher_suite *suite;
	/** Dynamically-allocated storage */
	void *dynamic;
	/** Bulk encryption cipher context */
	void *cipher_ctx;
	/** MAC secret */
	void *mac_secret;
	/** Fixed initialisation vector */
	void *fixed_iv;
};

/** A TLS cipher specification pair */
struct tls_cipherspec_pair {
	/** Current cipher specification */
	struct tls_cipherspec active;
	/** Next cipher specification */
	struct tls_cipherspec pending;
};

/** A TLS signature and hash algorithm identifier */
struct tls_signature_hash_id {
	/** Hash algorithm */
	uint8_t hash;
	/** Signature algorithm */
	uint8_t signature;
} __attribute__ (( packed ));

/** A TLS signature algorithm */
struct tls_signature_hash_algorithm {
	/** Digest algorithm */
	struct digest_algorithm *digest;
	/** Public-key algorithm */
	struct pubkey_algorithm *pubkey;
	/** Numeric code */
	struct tls_signature_hash_id code;
};

/** TLS signature hash algorithm table
 *
 * Note that the default (TLSv1.1 and earlier) algorithm using
 * MD5+SHA1 is never explicitly specified.
 */
#define TLS_SIG_HASH_ALGORITHMS						\
	__table ( struct tls_signature_hash_algorithm,			\
		  "tls_sig_hash_algorithms" )

/** Declare a TLS signature hash algorithm */
#define __tls_sig_hash_algorithm					\
	__table_entry ( TLS_SIG_HASH_ALGORITHMS, 01 )

/** TLS client random data */
struct tls_client_random {
	/** GMT Unix time */
	uint32_t gmt_unix_time;
	/** Random data */
	uint8_t random[28];
} __attribute__ (( packed ));

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

/** A TLS session */
struct tls_session {
	/** Reference counter */
	struct refcnt refcnt;
	/** List of sessions */
	struct list_head list;

	/** Server name */
	const char *name;
	/** Root of trust */
	struct x509_root *root;
	/** Private key */
	struct private_key *key;

	/** Session ID */
	uint8_t id[32];
	/** Length of session ID */
	size_t id_len;
	/** Session ticket */
	void *ticket;
	/** Length of session ticket */
	size_t ticket_len;
	/** Master secret */
	uint8_t master_secret[48];

	/** List of connections */
	struct list_head conn;
};

/** TLS transmit state */
struct tls_tx {
	/** Cipher specifications */
	struct tls_cipherspec_pair cipherspec;
	/** Sequence number */
	uint64_t seq;
	/** Pending transmissions */
	unsigned int pending;
	/** Transmit process */
	struct process process;
};

/** TLS receive state */
struct tls_rx {
	/** Cipher specifications */
	struct tls_cipherspec_pair cipherspec;
	/** Sequence number */
	uint64_t seq;
	/** State machine current state */
	enum tls_rx_state state;
	/** Current received record header */
	struct tls_header header;
	/** Current received record header (static I/O buffer) */
	struct io_buffer iobuf;
	/** List of received data buffers */
	struct list_head data;
	/** Received handshake fragment */
	struct io_buffer *handshake;
};

/** TLS client state */
struct tls_client {
	/** Random bytes */
	struct tls_client_random random;
	/** Private key (if used) */
	struct private_key *key;
	/** Certificate chain (if used) */
	struct x509_chain *chain;
	/** Security negotiation pending operation */
	struct pending_operation negotiation;
};

/** TLS server state */
struct tls_server {
	/** Random bytes */
	uint8_t random[32];
	/** Server Key Exchange record (if any) */
	void *exchange;
	/** Server Key Exchange record length */
	size_t exchange_len;
	/** Root of trust */
	struct x509_root *root;
	/** Certificate chain */
	struct x509_chain *chain;
	/** Public key (within server certificate) */
	struct asn1_cursor key;
	/** Certificate validator */
	struct interface validator;
	/** Certificate validation pending operation */
	struct pending_operation validation;
	/** Security negotiation pending operation */
	struct pending_operation negotiation;
};

/** A TLS connection */
struct tls_connection {
	/** Reference counter */
	struct refcnt refcnt;

	/** Session */
	struct tls_session *session;
	/** List of connections within the same session */
	struct list_head list;
	/** Session ID */
	uint8_t session_id[32];
	/** Length of session ID */
	size_t session_id_len;
	/** New session ticket */
	void *new_session_ticket;
	/** Length of new session ticket */
	size_t new_session_ticket_len;

	/** Plaintext stream */
	struct interface plainstream;
	/** Ciphertext stream */
	struct interface cipherstream;

	/** Protocol version */
	uint16_t version;
	/** Master secret */
	uint8_t master_secret[48];
	/** Digest algorithm used for handshake verification */
	struct digest_algorithm *handshake_digest;
	/** Digest algorithm context used for handshake verification */
	uint8_t *handshake_ctx;
	/** Secure renegotiation flag */
	int secure_renegotiation;
	/** Verification data */
	struct tls_verify_data verify;

	/** Transmit state */
	struct tls_tx tx;
	/** Receive state */
	struct tls_rx rx;
	/** Client state */
	struct tls_client client;
	/** Server state */
	struct tls_server server;
};

/** Advertised maximum fragment length */
#define TLS_MAX_FRAGMENT_LENGTH_VALUE TLS_MAX_FRAGMENT_LENGTH_4096

/** TX maximum fragment length
 *
 * TLS requires us to limit our transmitted records to the maximum
 * fragment length that we attempt to negotiate, even if the server
 * does not respect this choice.
 */
#define TLS_TX_BUFSIZE 4096

/** RX I/O buffer size
 *
 * The maximum fragment length extension is optional, and many common
 * implementations (including OpenSSL) do not support it.  We must
 * therefore be prepared to receive records of up to 16kB in length.
 * The chance of an allocation of this size failing is non-negligible,
 * so we must split received data into smaller allocations.
 */
#define TLS_RX_BUFSIZE 4096

/** Minimum RX I/O buffer size
 *
 * To simplify manipulations, we ensure that no RX I/O buffer is
 * smaller than this size.  This allows us to assume that the MAC and
 * padding are entirely contained within the final I/O buffer.
 */
#define TLS_RX_MIN_BUFSIZE 512

/** RX I/O buffer alignment */
#define TLS_RX_ALIGN 16

extern struct tls_key_exchange_algorithm tls_pubkey_exchange_algorithm;
extern struct tls_key_exchange_algorithm tls_dhe_exchange_algorithm;
extern struct tls_key_exchange_algorithm tls_ecdhe_exchange_algorithm;

extern int add_tls ( struct interface *xfer, const char *name,
		     struct x509_root *root, struct private_key *key );

#endif /* _IPXE_TLS_H */
