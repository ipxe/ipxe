/*
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

#include <config/crypto.h>

/** @file
 *
 * Cryptographic configuration
 *
 * Cryptographic configuration is slightly messy since we need to drag
 * in objects based on combinations of build options.
 */

PROVIDE_REQUIRING_SYMBOL();

/* RSA */
#if defined ( CRYPTO_PUBKEY_RSA )
REQUIRE_OBJECT ( oid_rsa );
#endif

/* MD4 */
#if defined ( CRYPTO_DIGEST_MD4 )
REQUIRE_OBJECT ( oid_md4 );
#endif

/* MD5 */
#if defined ( CRYPTO_DIGEST_MD5 )
REQUIRE_OBJECT ( oid_md5 );
#endif

/* SHA-1 */
#if defined ( CRYPTO_DIGEST_SHA1 )
REQUIRE_OBJECT ( oid_sha1 );
#endif

/* SHA-224 */
#if defined ( CRYPTO_DIGEST_SHA224 )
REQUIRE_OBJECT ( oid_sha224 );
#endif

/* SHA-256 */
#if defined ( CRYPTO_DIGEST_SHA256 )
REQUIRE_OBJECT ( oid_sha256 );
#endif

/* SHA-384 */
#if defined ( CRYPTO_DIGEST_SHA384 )
REQUIRE_OBJECT ( oid_sha384 );
#endif

/* SHA-512 */
#if defined ( CRYPTO_DIGEST_SHA512 )
REQUIRE_OBJECT ( oid_sha512 );
#endif

/* SHA-512/224 */
#if defined ( CRYPTO_DIGEST_SHA512_224 )
REQUIRE_OBJECT ( oid_sha512_224 );
#endif

/* SHA-512/256 */
#if defined ( CRYPTO_DIGEST_SHA512_256 )
REQUIRE_OBJECT ( oid_sha512_256 );
#endif

/* X25519 */
#if defined ( CRYPTO_CURVE_X25519 )
REQUIRE_OBJECT ( oid_x25519 );
#endif

/* P-256 */
#if defined ( CRYPTO_CURVE_P256 )
REQUIRE_OBJECT ( oid_p256 );
#endif

/* P-384 */
#if defined ( CRYPTO_CURVE_P384 )
REQUIRE_OBJECT ( oid_p384 );
#endif

/* AES-CBC */
#if defined ( CRYPTO_CIPHER_AES_CBC )
REQUIRE_OBJECT ( oid_aes_cbc );
#endif

/* AES-GCM */
#if defined ( CRYPTO_CIPHER_AES_GCM )
REQUIRE_OBJECT ( oid_aes_gcm );
#endif

/* RSA and MD5 */
#if defined ( CRYPTO_PUBKEY_RSA ) && defined ( CRYPTO_DIGEST_MD5 )
REQUIRE_OBJECT ( rsa_md5 );
#endif

/* RSA and SHA-1 */
#if defined ( CRYPTO_PUBKEY_RSA ) && defined ( CRYPTO_DIGEST_SHA1 )
REQUIRE_OBJECT ( rsa_sha1 );
#endif

/* RSA and SHA-224 */
#if defined ( CRYPTO_PUBKEY_RSA ) && defined ( CRYPTO_DIGEST_SHA224 )
REQUIRE_OBJECT ( rsa_sha224 );
#endif

/* RSA and SHA-256 */
#if defined ( CRYPTO_PUBKEY_RSA ) && defined ( CRYPTO_DIGEST_SHA256 )
REQUIRE_OBJECT ( rsa_sha256 );
#endif

/* RSA and SHA-384 */
#if defined ( CRYPTO_PUBKEY_RSA ) && defined ( CRYPTO_DIGEST_SHA384 )
REQUIRE_OBJECT ( rsa_sha384 );
#endif

/* RSA and SHA-512 */
#if defined ( CRYPTO_PUBKEY_RSA ) && defined ( CRYPTO_DIGEST_SHA512 )
REQUIRE_OBJECT ( rsa_sha512 );
#endif

/* RSA, AES-CBC, and SHA-1 */
#if defined ( CRYPTO_EXCHANGE_PUBKEY ) && defined ( CRYPTO_PUBKEY_RSA ) && \
    defined ( CRYPTO_CIPHER_AES_CBC ) && defined ( CRYPTO_DIGEST_SHA1 )
REQUIRE_OBJECT ( rsa_aes_cbc_sha1 );
#endif

/* RSA, AES-CBC, and SHA-256 */
#if defined ( CRYPTO_EXCHANGE_PUBKEY ) && defined ( CRYPTO_PUBKEY_RSA ) && \
    defined ( CRYPTO_CIPHER_AES_CBC ) && defined ( CRYPTO_DIGEST_SHA256 )
REQUIRE_OBJECT ( rsa_aes_cbc_sha256 );
#endif

/* RSA, AES-GCM, and SHA-256 */
#if defined ( CRYPTO_EXCHANGE_PUBKEY ) && defined ( CRYPTO_PUBKEY_RSA ) && \
    defined ( CRYPTO_CIPHER_AES_GCM ) && defined ( CRYPTO_DIGEST_SHA256 )
REQUIRE_OBJECT ( rsa_aes_gcm_sha256 );
#endif

/* RSA, AES-GCM, and SHA-384 */
#if defined ( CRYPTO_EXCHANGE_PUBKEY ) && defined ( CRYPTO_PUBKEY_RSA ) && \
    defined ( CRYPTO_CIPHER_AES_GCM ) && defined ( CRYPTO_DIGEST_SHA384 )
REQUIRE_OBJECT ( rsa_aes_gcm_sha384 );
#endif

/* DHE, RSA, AES-CBC, and SHA-1 */
#if defined ( CRYPTO_EXCHANGE_DHE ) && defined ( CRYPTO_PUBKEY_RSA ) && \
    defined ( CRYPTO_CIPHER_AES_CBC ) && defined ( CRYPTO_DIGEST_SHA1 )
REQUIRE_OBJECT ( dhe_rsa_aes_cbc_sha1 );
#endif

/* DHE, RSA, AES-CBC, and SHA-256 */
#if defined ( CRYPTO_EXCHANGE_DHE ) && defined ( CRYPTO_PUBKEY_RSA ) && \
    defined ( CRYPTO_CIPHER_AES_CBC ) && defined ( CRYPTO_DIGEST_SHA256 )
REQUIRE_OBJECT ( dhe_rsa_aes_cbc_sha256 );
#endif

/* DHE, RSA, AES-GCM, and SHA-256 */
#if defined ( CRYPTO_EXCHANGE_DHE ) && defined ( CRYPTO_PUBKEY_RSA ) && \
    defined ( CRYPTO_CIPHER_AES_GCM ) && defined ( CRYPTO_DIGEST_SHA256 )
REQUIRE_OBJECT ( dhe_rsa_aes_gcm_sha256 );
#endif

/* DHE, RSA, AES-GCM, and SHA-384 */
#if defined ( CRYPTO_EXCHANGE_DHE ) && defined ( CRYPTO_PUBKEY_RSA ) && \
    defined ( CRYPTO_CIPHER_AES_GCM ) && defined ( CRYPTO_DIGEST_SHA384 )
REQUIRE_OBJECT ( dhe_rsa_aes_gcm_sha384 );
#endif

/* ECDHE, RSA, AES-CBC, and SHA-1 */
#if defined ( CRYPTO_EXCHANGE_ECDHE ) && defined ( CRYPTO_PUBKEY_RSA ) && \
    defined ( CRYPTO_CIPHER_AES_CBC ) && defined ( CRYPTO_DIGEST_SHA1 )
REQUIRE_OBJECT ( ecdhe_rsa_aes_cbc_sha1 );
#endif

/* ECDHE, RSA, AES-CBC, and SHA-256 */
#if defined ( CRYPTO_EXCHANGE_ECDHE ) && defined ( CRYPTO_PUBKEY_RSA ) && \
    defined ( CRYPTO_CIPHER_AES_CBC ) && defined ( CRYPTO_DIGEST_SHA256 )
REQUIRE_OBJECT ( ecdhe_rsa_aes_cbc_sha256 );
#endif

/* ECDHE, RSA, AES-CBC, and SHA-384 */
#if defined ( CRYPTO_EXCHANGE_ECDHE ) && defined ( CRYPTO_PUBKEY_RSA ) && \
    defined ( CRYPTO_CIPHER_AES_CBC ) && defined ( CRYPTO_DIGEST_SHA384 )
REQUIRE_OBJECT ( ecdhe_rsa_aes_cbc_sha384 );
#endif

/* ECDHE, RSA, AES-GCM, and SHA-256 */
#if defined ( CRYPTO_EXCHANGE_ECDHE ) && defined ( CRYPTO_PUBKEY_RSA ) && \
    defined ( CRYPTO_CIPHER_AES_GCM ) && defined ( CRYPTO_DIGEST_SHA256 )
REQUIRE_OBJECT ( ecdhe_rsa_aes_gcm_sha256 );
#endif

/* ECDHE, RSA, AES-GCM, and SHA-384 */
#if defined ( CRYPTO_EXCHANGE_ECDHE ) && defined ( CRYPTO_PUBKEY_RSA ) && \
    defined ( CRYPTO_CIPHER_AES_GCM ) && defined ( CRYPTO_DIGEST_SHA384 )
REQUIRE_OBJECT ( ecdhe_rsa_aes_gcm_sha384 );
#endif
