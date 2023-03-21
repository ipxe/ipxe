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

/** @file
 *
 * PEM self-tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <string.h>
#include <assert.h>
#include <ipxe/test.h>
#include <ipxe/pem.h>
#include "asn1_test.h"

/** Define inline expected digest */
#define DIGEST(...) { { __VA_ARGS__ } }

/** Single RSA private key */
ASN1 ( single, &pem_image_type,
       "-----BEGIN RSA PRIVATE KEY-----\n"
       "MCwCAQACBQC6loItAgMBAAECBCqhYIkCAwDyVwIDAMUbAgMAr9kCAmr9AgIaWQ==\n"
       "-----END RSA PRIVATE KEY-----\n",
       DIGEST ( 0xb9, 0x38, 0x83, 0xcd, 0xf4, 0x58, 0xa9, 0xa2, 0x84, 0x11,
		0xfa, 0x0b, 0x6f, 0xdc, 0x3e, 0xa3, 0x7c, 0x90, 0x7c, 0x2d ) );

/** Three concatenated RSA private keys */
ASN1 ( multiple, &pem_image_type,
       "-----BEGIN RSA PRIVATE KEY-----\n"
       "MCwCAQACBQDtbjyVAgMBAAECBQCEOtJxAgMA+xsCAwDyDwICLGsCAgqTAgIxVQ==\n"
       "-----END RSA PRIVATE KEY-----\n"
       "-----BEGIN RSA PRIVATE KEY-----\n"
       "MCwCAQACBQC3VlyxAgMBAAECBGakxDUCAwDanwIDANavAgIBWQICTuECAwCmWg==\n"
       "-----END RSA PRIVATE KEY-----\n"
       "-----BEGIN RSA PRIVATE KEY-----\n"
       "MCwCAQACBQC89dS1AgMBAAECBQCxjnLBAgMA3qcCAwDZQwICP3cCAgpRAgI57A==\n"
       "-----END RSA PRIVATE KEY-----\n",
       DIGEST ( 0x9c, 0xb2, 0xc1, 0xa0, 0x9c, 0xcb, 0x11, 0xbf, 0x80, 0xd0,
		0x8c, 0xe5, 0xda, 0xf2, 0x3b, 0x2c, 0xca, 0x64, 0x25, 0x8a ),
       DIGEST ( 0x82, 0x66, 0x24, 0xd9, 0xc3, 0x98, 0x1e, 0x5e, 0x56, 0xed,
		0xd0, 0xd0, 0x2a, 0x5e, 0x9c, 0x3a, 0x58, 0xdf, 0x76, 0x0d ),
       DIGEST ( 0x01, 0xd2, 0x8a, 0x74, 0x42, 0x08, 0x0f, 0xb0, 0x03, 0x82,
		0xcd, 0xa3, 0xdc, 0x78, 0xfe, 0xd7, 0xa3, 0x28, 0xfc, 0x29 ) );

/** Two RSA private keys with various bits of noise added */
ASN1 ( noisy, &pem_image_type,
       "Hello world!  This is uninteresting stuff before the actual data.\n"
       "-----BEGIN RSA PRIVATE KEY-----\n"
       "MCwCAQACBQC3VlyxAgMBAAECBGakxDUCAwDanwIDANavAgIBWQICTuECAwCmWg==\n"
       "-----END RSA PRIVATE KEY-----\n"
       "Here is some more uninteresting stuff.\n"
       "Followed by what is actually another RSA private key, but with "
       "extra whitespace added, and the description change to pretend "
       "it's a certificate\n"
       "-----BEGIN CERTIFICATE-----\n"
       "   MCwCAQACBQC6loItAgMBAAECBCqhYIkCAwD\r\n"
       "   yVwIDAMUbAgMAr9kCAmr9AgIaWQ==  \r\n"
       "-----END CERTIFICATE-----\n"
       "and some trailing garbage as well\n"
       "and more garbage with no final newline",
       DIGEST ( 0x82, 0x66, 0x24, 0xd9, 0xc3, 0x98, 0x1e, 0x5e, 0x56, 0xed,
		0xd0, 0xd0, 0x2a, 0x5e, 0x9c, 0x3a, 0x58, 0xdf, 0x76, 0x0d ),
       DIGEST ( 0xb9, 0x38, 0x83, 0xcd, 0xf4, 0x58, 0xa9, 0xa2, 0x84, 0x11,
		0xfa, 0x0b, 0x6f, 0xdc, 0x3e, 0xa3, 0x7c, 0x90, 0x7c, 0x2d ) );

/**
 * Perform PEM self-test
 *
 */
static void pem_test_exec ( void ) {

	/* Perform tests */
	asn1_ok ( &single );
	asn1_ok ( &multiple );
	asn1_ok ( &noisy );
}

/** PEM self-test */
struct self_test pem_test __self_test = {
	.name = "pem",
	.exec = pem_test_exec,
};
