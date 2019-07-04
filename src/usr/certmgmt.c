/*
 * Copyright (C) 2016 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdio.h>
#include <errno.h>
#include <ipxe/x509.h>
#include <ipxe/sha1.h>
#include <ipxe/base16.h>
#include <usr/certmgmt.h>

/** @file
 *
 * Certificate management
 *
 */

/**
 * Display status of a certificate
 *
 * @v cert		X.509 certificate
 */
void certstat ( struct x509_certificate *cert ) {
	struct digest_algorithm *digest = &sha1_algorithm;
	uint8_t fingerprint[ digest->digestsize ];
	char buf[ base16_encoded_len ( sizeof ( fingerprint ) ) + 1 /* NUL */ ];

	/* Generate fingerprint */
	x509_fingerprint ( cert, digest, fingerprint );
	base16_encode ( fingerprint, sizeof ( fingerprint ),
			buf, sizeof ( buf ) );

	/* Print certificate status */
	printf ( "%s : %s", x509_name ( cert ), buf );
	if ( cert->flags & X509_FL_PERMANENT )
		printf ( " [PERMANENT]" );
	if ( cert->flags & X509_FL_EXPLICIT )
		printf ( " [EXPLICIT]" );
	if ( x509_is_valid ( cert ) )
		printf ( " [VALIDATED]" );
	printf ( "\n" );
}
