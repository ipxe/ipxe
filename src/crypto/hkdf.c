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
 * HMAC-based Extract-and-Expand Key Derivation Function (HKDF)
 *
 * The algorithm is defined in RFC 5869, with its usage by TLS 1.3
 * documented in RFC 8446 section 7.1.
 *
 */

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <ipxe/crypto.h>
#include <ipxe/hmac.h>
#include <ipxe/hkdf.h>

/**
 * Extract fixed-length pseudorandom key
 *
 * @v digest		Digest algorithm
 * @v salt		Salt value (or NULL)
 * @v salt_len		Length of salt value
 * @v ikm		Input keying material
 * @v ikm_len		Length of input keying material
 * @v prk		Pseudorandom key to fill in
 */
void hkdf_extract ( struct digest_algorithm *digest, const void *salt,
		    size_t salt_len, const void *ikm, size_t ikm_len,
		    void *prk ) {
	uint8_t ctx[ hmac_ctxsize ( digest ) ];

	/* Use all-zero salt if not specified */
	if ( ! salt ) {
		assert ( salt_len == 0 );
		salt_len = digest->digestsize;
		memset ( prk, 0, salt_len );
		salt = prk;
	}

	/* Calculate pseudorandom key */
	hmac_init ( digest, ctx, salt, salt_len );
	hmac_update ( digest, ctx, ikm, ikm_len );
	hmac_final ( digest, ctx, prk );
}

/**
 * Expand pseudorandom key
 *
 * @v digest		Digest algorithm
 * @v prk		Pseudorandom key
 * @v info		Additional information (or NULL)
 * @v info_len		Length of additional information
 * @v out		Output keying material
 * @v len		Length of output keying material
 */
void hkdf_expand ( struct digest_algorithm *digest, const void *prk,
		   const void *info, size_t info_len, void *out, size_t len ) {
	size_t digestsize = digest->digestsize;
	uint8_t ctx[ hmac_ctxsize ( digest ) ];
	uint8_t hash[digestsize];
	uint8_t index = 0;
	size_t frag_len;

	while ( len ) {

		/* Calculate T(n) */
		hmac_init ( digest, ctx, prk, digestsize );
		if ( index++ )
			hmac_update ( digest, ctx, hash, digestsize );
		hmac_update ( digest, ctx, info, info_len );
		hmac_update ( digest, ctx, &index, sizeof ( index ) );
		hmac_final ( digest, ctx, hash );

		/* Copy to output buffer */
		frag_len = len;
		if ( frag_len > digestsize )
			frag_len = digestsize;
		memcpy ( out, hash, frag_len );
		out += frag_len;
		len -= frag_len;
	}
}
