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
 * SHA-256 tests
 *
 * NIST test vectors are taken from
 *
 *  http://csrc.nist.gov/groups/ST/toolkit/documents/Examples/SHA256.pdf
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <ipxe/sha256.h>
#include <ipxe/test.h>
#include "digest_test.h"

/* Empty test vector (digest obtained from "sha256sum /dev/null") */
DIGEST_TEST ( sha256_empty, &sha256_algorithm, DIGEST_EMPTY,
	      DIGEST ( 0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a,
		       0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24, 0x27, 0xae,
		       0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c, 0xa4, 0x95, 0x99,
		       0x1b, 0x78, 0x52, 0xb8, 0x55 ) );

/* NIST test vector "abc" */
DIGEST_TEST ( sha256_nist_abc, &sha256_algorithm, DIGEST_NIST_ABC,
	      DIGEST ( 0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41,
		       0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03,
		       0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff,
		       0x61, 0xf2, 0x00, 0x15, 0xad ) );

/* NIST test vector "abc...opq" */
DIGEST_TEST ( sha256_nist_abc_opq, &sha256_algorithm, DIGEST_NIST_ABC_OPQ,
	      DIGEST ( 0x24, 0x8d, 0x6a, 0x61, 0xd2, 0x06, 0x38, 0xb8, 0xe5,
		       0xc0, 0x26, 0x93, 0x0c, 0x3e, 0x60, 0x39, 0xa3, 0x3c,
		       0xe4, 0x59, 0x64, 0xff, 0x21, 0x67, 0xf6, 0xec, 0xed,
		       0xd4, 0x19, 0xdb, 0x06, 0xc1 ) );

/**
 * Perform SHA-256 self-test
 *
 */
static void sha256_test_exec ( void ) {

	/* Correctness tests */
	digest_ok ( &sha256_empty );
	digest_ok ( &sha256_nist_abc );
	digest_ok ( &sha256_nist_abc_opq );

	/* Speed tests */
	DBG ( "SHA256 required %ld cycles per byte\n",
	      digest_cost ( &sha256_algorithm ) );
}

/** SHA-256 self-test */
struct self_test sha256_test __self_test = {
	.name = "sha256",
	.exec = sha256_test_exec,
};
