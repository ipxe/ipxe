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

/** @file
 *
 * Archive extraction self-tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <stdlib.h>
#include <string.h>
#include <ipxe/image.h>
#include "archive_test.h"

/**
 * Report an archive extraction test result
 *
 * @v test		Archive extraction test
 * @v file		Test code file
 * @v line		Test code line
 */
void archive_okx ( struct archive_test *test, const char *file,
		   unsigned int line ) {
	struct image *image;
	struct image *extracted;

	/* Construct archive image */
	image = image_memory ( test->archive_name, test->archive,
			       test->archive_len );
	okx ( image != NULL, file, line );
	okx ( image->len == test->archive_len, file, line );
	DBGC ( test, "ARCHIVE %s is:\n", test->archive_name );
	DBGC_HDA ( test, 0, image->data, image->len );

	/* Check type detection */
	okx ( image->type == test->type, file, line );

	/* Extract archive image */
	okx ( image_extract ( image, test->extract_name, &extracted ) == 0,
	      file, line );
	DBGC ( test, "ARCHIVE %s extracted:\n", test->archive_name );
	DBGC_HDA ( test, 0, extracted->data, extracted->len );

	/* Verify extracted image content */
	okx ( extracted->len == test->expected_len, file, line );
	okx ( memcmp ( extracted->data, test->expected,
		       test->expected_len ) == 0, file, line );

	/* Verify extracted image name */
	okx ( strcmp ( extracted->name, test->expected_name ) == 0,
	      file, line );

	/* Unregister images */
	unregister_image ( extracted );
	unregister_image ( image );
}
