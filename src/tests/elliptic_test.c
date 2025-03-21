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
 * Elliptic curve self-tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <ipxe/crypto.h>
#include <ipxe/test.h>
#include "elliptic_test.h"

/**
 * Report elliptic curve point multiplication test result
 *
 * @v test		Elliptic curve point multiplication test
 * @v file		Test code file
 * @v line		Test code line
 */
void elliptic_okx ( struct elliptic_test *test, const char *file,
		    unsigned int line ) {
	struct elliptic_curve *curve = test->curve;
	size_t pointsize = curve->pointsize;
	size_t keysize = curve->keysize;
	uint8_t actual[pointsize];
	int rc;

	/* Sanity checks */
	okx ( ( test->base_len == pointsize ) || ( ! test->base_len ),
	      file, line );
	okx ( test->scalar_len == keysize, file, line );
	okx ( ( test->expected_len == pointsize ) || ( ! test->expected_len ),
	      file, line );

	/* Perform point multiplication */
	rc = elliptic_multiply ( curve, ( test->base_len ? test->base : NULL ),
				 test->scalar, actual );
	if ( test->expected_len ) {
		okx ( rc == 0, file, line );
	} else {
		okx ( rc != 0, file, line );
	}

	/* Check expected result */
	okx ( memcmp ( actual, test->expected, test->expected_len ) == 0,
	      file, line );
}
