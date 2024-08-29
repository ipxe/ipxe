/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <string.h>
#include <syslog.h>
#include <ipxe/image.h>
#include <ipxe/cms.h>
#include <ipxe/privkey.h>
#include <usr/imgcrypt.h>

/** @file
 *
 * Image encryption management
 *
 */

/**
 * Decrypt image using downloaded envelope
 *
 * @v image		Image to decrypt
 * @v envelope		Image containing decryption key
 * @v name		Decrypted image name (or NULL to use default)
 * @ret rc		Return status code
 */
int imgdecrypt ( struct image *image, struct image *envelope,
		 const char *name ) {
	struct cms_message *cms;
	int rc;

	/* Parse envelope */
	if ( ( rc = cms_message ( envelope, &cms ) ) != 0 )
		goto err_parse;

	/* Decrypt image */
	if ( ( rc = cms_decrypt ( cms, image, name, &private_key ) ) != 0 )
		goto err_decrypt;

	/* Drop reference to message */
	cms_put ( cms );
	cms = NULL;

	/* Record decryption */
	syslog ( LOG_NOTICE, "Image \"%s\" decrypted OK\n", image->name );

	return 0;

 err_decrypt:
	cms_put ( cms );
 err_parse:
	syslog ( LOG_ERR, "Image \"%s\" decryption failed: %s\n",
		 image->name, strerror ( rc ) );
	return rc;
}
