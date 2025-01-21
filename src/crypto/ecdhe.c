/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
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

/** @file
 *
 * Elliptic Curve Ephemeral Diffie-Hellman (ECDHE) key exchange
 *
 */

#include <string.h>
#include <ipxe/ecdhe.h>

/**
 * Calculate ECDHE key
 *
 * @v curve		Elliptic curve
 * @v partner		Partner public curve point
 * @v private		Private key
 * @v public		Public curve point to fill in (may overlap partner key)
 * @v shared		Shared secret curve point to fill in
 * @ret rc		Return status code
 */
int ecdhe_key ( struct elliptic_curve *curve, const void *partner,
		const void *private, void *public, void *shared ) {
	int rc;

	/* Construct shared key */
	if ( ( rc = elliptic_multiply ( curve, partner, private,
					shared ) ) != 0 ) {
		DBGC ( curve, "CURVE %s could not generate shared key: %s\n",
		       curve->name, strerror ( rc ) );
		return rc;
	}

	/* Construct public key */
	if ( ( rc = elliptic_multiply ( curve, NULL, private,
					public ) ) != 0 ) {
		DBGC ( curve, "CURVE %s could not generate public key: %s\n",
		       curve->name, strerror ( rc ) );
		return rc;
	}

	return 0;
}
