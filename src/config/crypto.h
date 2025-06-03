#ifndef CONFIG_CRYPTO_H
#define CONFIG_CRYPTO_H

/** @file
 *
 * Cryptographic configuration
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** Minimum TLS version */
#define TLS_VERSION_MIN TLS_VERSION_TLS_1_1

/** Public-key exchange algorithm */
#define CRYPTO_EXCHANGE_PUBKEY

/** DHE key exchange algorithm */
#define CRYPTO_EXCHANGE_DHE

/** ECDHE key exchange algorithm */
#define CRYPTO_EXCHANGE_ECDHE

/** RSA public-key algorithm */
#define CRYPTO_PUBKEY_RSA

/** AES-CBC block cipher */
#define CRYPTO_CIPHER_AES_CBC

/** AES-GCM block cipher */
#define CRYPTO_CIPHER_AES_GCM

/** MD4 digest algorithm */
//#define CRYPTO_DIGEST_MD4

/** MD5 digest algorithm */
//#define CRYPTO_DIGEST_MD5

/** SHA-1 digest algorithm */
#define CRYPTO_DIGEST_SHA1

/** SHA-224 digest algorithm */
#define CRYPTO_DIGEST_SHA224

/** SHA-256 digest algorithm */
#define CRYPTO_DIGEST_SHA256

/** SHA-384 digest algorithm */
#define CRYPTO_DIGEST_SHA384

/** SHA-512 digest algorithm */
#define CRYPTO_DIGEST_SHA512

/** SHA-512/224 digest algorithm */
//#define CRYPTO_DIGEST_SHA512_224

/** SHA-512/256 digest algorithm */
//#define CRYPTO_DIGEST_SHA512_256

/** X25519 elliptic curve */
#define CRYPTO_CURVE_X25519

/** P-256 elliptic curve */
#define CRYPTO_CURVE_P256

/** P-384 elliptic curve */
#define CRYPTO_CURVE_P384

/** Margin of error (in seconds) allowed in signed timestamps
 *
 * We default to allowing a reasonable margin of error: 12 hours to
 * allow for the local time zone being non-GMT, plus 30 minutes to
 * allow for general clock drift.
 */
#define TIMESTAMP_ERROR_MARGIN ( ( 12 * 60 + 30 ) * 60 )

/** Default cross-signed certificate source
 *
 * This is the default location from which iPXE will attempt to
 * download cross-signed certificates in order to complete a
 * certificate chain.
 */
#define CROSSCERT "http://ca.ipxe.org/auto"

/** Perform OCSP checks when applicable
 *
 * Some CAs provide non-functional OCSP servers, and some clients are
 * forced to operate on networks without access to the OCSP servers.
 * Allow the user to explicitly disable the use of OCSP checks.
 */
#define OCSP_CHECK

#include <config/named.h>
#include NAMED_CONFIG(crypto.h)
#include <config/local/crypto.h>
#include LOCAL_NAMED_CONFIG(crypto.h)

#endif /* CONFIG_CRYPTO_H */
