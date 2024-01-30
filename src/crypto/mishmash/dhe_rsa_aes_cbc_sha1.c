/*
 * Copyright (C) 2015 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <byteswap.h>
#include <ipxe/rsa.h>
#include <ipxe/aes.h>
#include <ipxe/sha1.h>
#include <ipxe/sha256.h>
#include <ipxe/tls.h>

/** TLS_DHE_RSA_WITH_AES_128_CBC_SHA cipher suite */
struct tls_cipher_suite
tls_dhe_rsa_with_aes_128_cbc_sha __tls_cipher_suite ( 15 ) = {
	.code = htons ( TLS_DHE_RSA_WITH_AES_128_CBC_SHA ),
	.key_len = ( 128 / 8 ),
	.fixed_iv_len = 0,
	.record_iv_len = AES_BLOCKSIZE,
	.mac_len = SHA1_DIGEST_SIZE,
	.exchange = &tls_dhe_exchange_algorithm,
	.pubkey = &rsa_algorithm,
	.cipher = &aes_cbc_algorithm,
	.digest = &sha1_algorithm,
	.handshake = &sha256_algorithm,
};

/** TLS_DHE_RSA_WITH_AES_256_CBC_SHA cipher suite */
struct tls_cipher_suite
tls_dhe_rsa_with_aes_256_cbc_sha __tls_cipher_suite ( 16 ) = {
	.code = htons ( TLS_DHE_RSA_WITH_AES_256_CBC_SHA ),
	.key_len = ( 256 / 8 ),
	.fixed_iv_len = 0,
	.record_iv_len = AES_BLOCKSIZE,
	.mac_len = SHA1_DIGEST_SIZE,
	.exchange = &tls_dhe_exchange_algorithm,
	.pubkey = &rsa_algorithm,
	.cipher = &aes_cbc_algorithm,
	.digest = &sha1_algorithm,
	.handshake = &sha256_algorithm,
};
