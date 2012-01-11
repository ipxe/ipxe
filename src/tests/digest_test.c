/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

/** @file
 *
 * Digest self-tests
 *
 */

#include <string.h>
#include <ipxe/crypto.h>
#include "digest_test.h"

/**
 * Test digest algorithm
 *
 * @v digest		Digest algorithm
 * @v fragments		Digest test fragment list, or NULL
 * @v data		Test data
 * @v len		Length of test data
 * @v expected		Expected digest value
 * @ret ok		Digest value is as expected
 */
int digest_test ( struct digest_algorithm *digest,
		  struct digest_test_fragments *fragments,
		  void *data, size_t len, void *expected ) {
	uint8_t ctx[digest->ctxsize];
	uint8_t out[digest->digestsize];
	size_t frag_len = 0;
	unsigned int i;

	/* Initialise digest */
	digest_init ( digest, ctx );

	/* Update digest fragment-by-fragment */
	for ( i = 0 ; len && ( i < ( sizeof ( fragments->len ) /
				     sizeof ( fragments->len[0] ) ) ) ; i++ ) {
		if ( fragments )
			frag_len = fragments->len[i];
		if ( ( frag_len == 0 ) || ( frag_len < len ) )
			frag_len = len;
		digest_update ( digest, ctx, data, frag_len );
		data += frag_len;
		len -= frag_len;
	}

	/* Finalise digest */
	digest_final ( digest, ctx, out );

	/* Compare against expected output */
	return ( memcmp ( expected, out, sizeof ( out ) ) == 0 );
}
