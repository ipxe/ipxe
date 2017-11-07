/*
 * Copyright (C) 2017 Michael Brown <mbrown@fensystems.co.uk>.
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
 * MD4 tests
 *
 * Test inputs borrowed from NIST SHA-1 tests, with results calculated
 * using "openssl dgst -md4"
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <ipxe/md4.h>
#include <ipxe/test.h>
#include "digest_test.h"

/* Empty test vector */
DIGEST_TEST ( md4_empty, &md4_algorithm, DIGEST_EMPTY,
	      DIGEST ( 0x31, 0xd6, 0xcf, 0xe0, 0xd1, 0x6a, 0xe9, 0x31, 0xb7,
		       0x3c, 0x59, 0xd7, 0xe0, 0xc0, 0x89, 0xc0 ) );

/* NIST test vector "abc" */
DIGEST_TEST ( md4_nist_abc, &md4_algorithm, DIGEST_NIST_ABC,
	      DIGEST ( 0xa4, 0x48, 0x01, 0x7a, 0xaf, 0x21, 0xd8, 0x52, 0x5f,
		       0xc1, 0x0a, 0xe8, 0x7a, 0xa6, 0x72, 0x9d ) );

/* NIST test vector "abc...opq" */
DIGEST_TEST ( md4_nist_abc_opq, &md4_algorithm, DIGEST_NIST_ABC_OPQ,
	      DIGEST ( 0x46, 0x91, 0xa9, 0xec, 0x81, 0xb1, 0xa6, 0xbd, 0x1a,
		       0xb8, 0x55, 0x72, 0x40, 0xb2, 0x45, 0xc5 ) );

/**
 * Perform MD4 self-test
 *
 */
static void md4_test_exec ( void ) {

	/* Correctness tests */
	digest_ok ( &md4_empty );
	digest_ok ( &md4_nist_abc );
	digest_ok ( &md4_nist_abc_opq );

	/* Speed tests */
	DBG ( "MD4 required %ld cycles per byte\n",
	      digest_cost ( &md4_algorithm ) );
}

/** MD4 self-test */
struct self_test md4_test __self_test = {
	.name = "md4",
	.exec = md4_test_exec,
};
