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
 * ASN.1 self-tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <stdlib.h>
#include <assert.h>
#include <ipxe/image.h>
#include <ipxe/asn1.h>
#include <ipxe/test.h>
#include "asn1_test.h"

/**
 * Report ASN.1 test result
 *
 * @v test		ASN.1 test
 * @v file		Test code file
 * @v line		Test code line
 */
void asn1_okx ( struct asn1_test *test, const char *file, unsigned int line ) {
	struct digest_algorithm *digest = &asn1_test_digest_algorithm;
	struct asn1_cursor *cursor;
	uint8_t ctx[digest->ctxsize];
	uint8_t out[ASN1_TEST_DIGEST_SIZE];
	unsigned int i;
	size_t offset;
	int next;

	/* Sanity check */
	assert ( sizeof ( out ) == digest->digestsize );

	/* Correct image data pointer */
	test->image->data = virt_to_user ( ( void * ) test->image->data );

	/* Check that image is detected as correct type */
	okx ( register_image ( test->image ) == 0, file, line );
	okx ( test->image->type == test->type, file, line );

	/* Check that all ASN.1 objects can be extracted */
	for ( offset = 0, i = 0 ; i < test->count ; offset = next, i++ ) {

		/* Extract ASN.1 object */
		next = image_asn1 ( test->image, offset, &cursor );
		okx ( next >= 0, file, line );
		okx ( ( ( size_t ) next ) > offset, file, line );
		if ( next > 0 ) {

			/* Calculate digest of ASN.1 object */
			digest_init ( digest, ctx );
			digest_update ( digest, ctx, cursor->data,
					cursor->len );
			digest_final ( digest, ctx, out );

			/* Compare against expected digest */
			okx ( memcmp ( out, test->expected[i].digest,
				       sizeof ( out ) ) == 0, file, line );

			/* Free ASN.1 object */
			free ( cursor );
		}
	}

	/* Check that we have reached the end of the image */
	okx ( offset == test->image->len, file, line );

	/* Unregister image */
	unregister_image ( test->image );
}
