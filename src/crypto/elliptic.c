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
 * Elliptic curves
 *
 */

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <ipxe/crypto.h>
#include <ipxe/elliptic.h>

/**
 * Share public key
 *
 * @v exchange		Key exchange algorithm
 * @v private		Private key
 * @v public		Public key to fill in
 */
void elliptic_share ( struct exchange_algorithm *exchange, const void *private,
		      void *public ) {
	struct elliptic_curve *curve = exchange->priv;
	size_t len = exchange->sharedsize;
	elliptic_uncompressed_t ( len ) *result = public;
	int rc;

	/* Sanity checks */
	assert ( curve->keysize == exchange->privsize );
	assert ( curve->pointsize == sizeof ( result->xy ) );
	assert ( sizeof ( *result ) == exchange->pubsize );

	/* Calculate public key */
	result->format = ELLIPTIC_FORMAT_UNCOMPRESSED;
	rc = elliptic_multiply ( curve, curve->base, private, &result->xy );

	/* Can never fail when using the curve's own base point */
	assert ( rc == 0 );
}

/**
 * Agree shared secret
 *
 * @v exchange		Key exchange algorithm
 * @v private		Private key
 * @v partner		Partner public key
 * @v shared		Shared secret to fill in
 * @ret rc		Return status code
 */
int elliptic_agree ( struct exchange_algorithm *exchange, const void *private,
		     const void *partner, void *shared ) {
	struct elliptic_curve *curve = exchange->priv;
	size_t len = exchange->sharedsize;
	const elliptic_uncompressed_t ( len ) *base = partner;
	elliptic_uncompressed_t ( len ) result;
	int rc;

	/* Sanity checks */
	assert ( curve->keysize == exchange->privsize );
	assert ( curve->pointsize == sizeof ( base->xy ) );
	assert ( curve->pointsize == sizeof ( result.xy ) );
	assert ( sizeof ( *base ) == exchange->pubsize );
	assert ( sizeof ( result.x ) == exchange->sharedsize );

	/* Calculate shared secret */
	if ( ( rc = elliptic_multiply ( curve, &base->xy, private,
					result.xy ) ) != 0 )
		return rc;
	memcpy ( shared, &result.x, sizeof ( result.x ) );

	/* Check for point at infinity */
	if ( elliptic_is_infinity ( curve, &result.xy ) )
		return -EPERM;

	/* Check format byte */
	if ( base->format != ELLIPTIC_FORMAT_UNCOMPRESSED )
		return -EPERM;

	return 0;
}
