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
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 *
 * Alternatively, you may distribute this code in source or binary
 * form, with or without modification, provided that the following
 * conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the above disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the above
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

/**
 * @file
 *
 * Keyed-Hashing for Message Authentication
 */

#include <string.h>
#include <assert.h>
#include <ipxe/crypto.h>
#include <ipxe/hmac.h>

/**
 * Construct HMAC reduced key
 *
 * @v digest		Digest algorithm to use
 * @v ctx		HMAC context
 * @v secret		Secret key
 * @v len		Length of secret key
 * @v key		HMAC reduced key to fill in
 */
void hmac_key ( struct digest_algorithm *digest, void *ctx, const void *secret,
		size_t len, void *key ) {
	hmac_context_t ( digest ) *hctx = ctx;
	hmac_key_t ( digest ) *hkey = key;

	/* Reduce key (if applicable) and zero-pad to block size */
	memset ( hkey->pad, 0, sizeof ( hkey->pad ) );
	if ( len <= sizeof ( hkey->pad ) ) {
		memcpy ( hkey->pad, secret, len );
	} else {
		digest_init ( digest, hctx->ctx );
		digest_update ( digest, hctx->ctx, secret, len );
		digest_final ( digest, hctx->ctx, hkey->pad );
	}
}

/**
 * Initialise HMAC from reduced key
 *
 * @v digest		Digest algorithm
 * @v ctx		HMAC context
 * @v hkey		HMAC key
 */
void hmac_init_key ( struct digest_algorithm *digest, void *ctx,
		     const void *key ) {
	hmac_context_t ( digest ) *hctx = ctx;
	const hmac_key_t ( digest ) *hkey = key;
	unsigned int i;

	/* Construct input pad */
	for ( i = 0 ; i < sizeof ( hkey->pad ) ; i++ ) {
		hctx->pad[i] = ( hkey->pad[i] ^ 0x36 );
	}

	/* Start inner hash */
	digest_init ( digest, hctx->ctx );
	digest_update ( digest, hctx->ctx, hctx->pad, sizeof ( hctx->pad ) );
}

/**
 * Initialise HMAC
 *
 * @v digest		Digest algorithm to use
 * @v ctx		HMAC context
 * @v secret		Secret key
 * @v len		Length of secret key
 */
void hmac_init ( struct digest_algorithm *digest, void *ctx,
		 const void *secret, size_t len ) {
	hmac_context_t ( digest ) *hctx = ctx;

	/* Construct reduced key (in input pad) */
	hmac_key ( digest, hctx->ctx, secret, len, hctx->pad );

	/* Initialise HMAC */
	hmac_init_key ( digest, hctx->ctx, hctx->pad );
}

/**
 * Finalise HMAC
 *
 * @v digest		Digest algorithm to use
 * @v ctx		HMAC context
 * @v hmac		HMAC digest to fill in
 */
void hmac_final ( struct digest_algorithm *digest, void *ctx, void *hmac ) {
	hmac_context_t ( digest ) *hctx = ctx;
	unsigned int i;

	/* Construct output pad from input pad */
	for ( i = 0 ; i < sizeof ( hctx->pad ) ; i++ ) {
		hctx->pad[i] ^= 0x6a;
	}

	/* Finish inner hash */
	digest_final ( digest, hctx->ctx, hmac );

	/* Perform outer hash */
	digest_init ( digest, hctx->ctx );
	digest_update ( digest, hctx->ctx, hctx->pad, sizeof ( hctx->pad ) );
	digest_update ( digest, hctx->ctx, hmac, digest->digestsize );
	digest_final ( digest, hctx->ctx, hmac );

	/* Erase output pad (from which the key may be derivable) */
	memset ( hctx->pad, 0, sizeof ( hctx->pad ) );
}
