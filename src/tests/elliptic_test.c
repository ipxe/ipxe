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
#include <ipxe/bigint.h>
#include <ipxe/crypto.h>
#include <ipxe/test.h>
#include "elliptic_test.h"

/**
 * Report elliptic curve sanity test result
 *
 * @v curve		Elliptic curve
 * @v file		Test code file
 * @v line		Test code line
 */
void elliptic_curve_okx ( struct elliptic_curve *curve, const char *file,
			  unsigned int line ) {
	static const uint8_t one[] = { 1 };
	size_t pointsize = curve->pointsize;
	size_t keysize = curve->keysize;
	uint8_t point[pointsize];
	uint8_t scalar[keysize];
	struct {
		bigint_t ( bigint_required_size ( keysize ) ) scalar;
		bigint_t ( bigint_required_size ( keysize ) ) one;
	} temp;

	/* Check that curve has the required properties */
	okx ( curve->base != NULL, file, line );
	okx ( curve->order != NULL, file, line );
	okx ( ( ! elliptic_is_infinity ( curve, curve->base ) ), file, line );

	/* Test multiplying base point by group order.  Result should
	 * be the point at infinity.
	 */
	okx ( elliptic_multiply ( curve, curve->base, curve->order,
				  point ) == 0, file, line );
	okx ( elliptic_is_infinity ( curve, point ), file, line );

	/* Test multiplying base point by group order plus one, to get
	 * back to the base point.
	 */
	bigint_init ( &temp.scalar, curve->order, keysize );
	bigint_init ( &temp.one, one, sizeof ( one ) );
	bigint_add ( &temp.one, &temp.scalar );
	bigint_done ( &temp.scalar, scalar, sizeof ( scalar ) );
	okx ( elliptic_multiply ( curve, curve->base, scalar, point ) == 0,
	      file, line );
	okx ( memcmp ( point, curve->base, pointsize ) == 0, file, line );
}

/**
 * Report elliptic curve point multiplication test result
 *
 * @v test		Elliptic curve point multiplication test
 * @v file		Test code file
 * @v line		Test code line
 */
void elliptic_multiply_okx ( struct elliptic_multiply_test *test,
			     const char *file, unsigned int line ) {
	struct elliptic_curve *curve = test->curve;
	size_t pointsize = curve->pointsize;
	size_t keysize = curve->keysize;
	uint8_t actual[pointsize];
	const void *base;
	int rc;

	/* Sanity checks */
	okx ( ( test->base_len == pointsize ) || ( ! test->base_len ),
	      file, line );
	okx ( test->scalar_len == keysize, file, line );
	okx ( ( test->expected_len == pointsize ) || ( ! test->expected_len ),
	      file, line );

	/* Perform point multiplication */
	base = ( test->base_len ? test->base : curve->base );
	rc = elliptic_multiply ( curve, base, test->scalar, actual );
	if ( test->expected_len ) {
		okx ( rc == 0, file, line );
	} else {
		okx ( rc != 0, file, line );
	}

	/* Check expected result */
	okx ( memcmp ( actual, test->expected, test->expected_len ) == 0,
	      file, line );
}

/**
 * Report elliptic curve point addition test result
 *
 * @v test		Elliptic curve point addition test
 * @v file		Test code file
 * @v line		Test code line
 */
void elliptic_add_okx ( struct elliptic_add_test *test,
			const char *file, unsigned int line ) {
	struct elliptic_curve *curve = test->curve;
	size_t pointsize = curve->pointsize;
	uint8_t actual[pointsize];
	int rc;

	/* Sanity checks */
	okx ( test->addend_len == pointsize, file, line );
	okx ( test->augend_len == pointsize, file, line );
	okx ( ( test->expected_len == pointsize ) || ( ! test->expected_len ),
	      file, line );

	/* Perform point addition */
	rc = elliptic_add ( curve, test->addend, test->augend, actual );
	if ( test->expected_len ) {
		okx ( rc == 0, file, line );
	} else {
		okx ( rc != 0, file, line );
	}

	/* Check expected result */
	okx ( memcmp ( actual, test->expected, test->expected_len ) == 0,
	      file, line );
}
