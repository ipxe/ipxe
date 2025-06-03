/*
 * Copyright (C) 2021 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <errno.h>
#include <ipxe/image.h>

/** @file
 *
 * Archive images
 *
 */

/**
 * Extract archive image
 *
 * @v image		Image
 * @v name		Extracted image name
 * @v extracted		Extracted image to fill in
 * @ret rc		Return status code
 */
int image_extract ( struct image *image, const char *name,
		    struct image **extracted ) {
	int rc;

	/* Check that this image can be used to extract an archive image */
	if ( ! ( image->type && image->type->extract ) ) {
		rc = -ENOTSUP;
		goto err_unsupported;
	}

	/* Allocate new image */
	*extracted = alloc_image ( image->uri );
	if ( ! *extracted ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Set image name */
	if ( ( rc = image_set_name ( *extracted,
				     ( name ? name : image->name ) ) ) != 0 ) {
		goto err_set_name;
	}

	/* Strip any archive or compression suffix from implicit name */
	if ( ! name )
		image_strip_suffix ( *extracted );

	/* Try extracting archive image */
	if ( ( rc = image->type->extract ( image, *extracted ) ) != 0 ) {
		DBGC ( image, "IMAGE %s could not extract image: %s\n",
		       image->name, strerror ( rc ) );
		goto err_extract;
	}

	/* Register image */
	if ( ( rc = register_image ( *extracted ) ) != 0 )
		goto err_register;

	/* Propagate trust flag */
	if ( image->flags & IMAGE_TRUSTED )
		image_trust ( *extracted );

	/* Drop local reference to image */
	image_put ( *extracted );

	return 0;

	unregister_image ( *extracted );
 err_register:
 err_extract:
 err_set_name:
	image_put ( *extracted );
 err_alloc:
 err_unsupported:
	return rc;
}

/**
 * Extract and execute image
 *
 * @v image		Image
 * @ret rc		Return status code
 */
int image_extract_exec ( struct image *image ) {
	struct image *extracted;
	int rc;

	/* Extract image */
	if ( ( rc = image_extract ( image, NULL, &extracted ) ) != 0 )
		goto err_extract;

	/* Set image command line */
	if ( ( rc = image_set_cmdline ( extracted, image->cmdline ) ) != 0 )
		goto err_set_cmdline;

	/* Set auto-unregister flag */
	extracted->flags |= IMAGE_AUTO_UNREGISTER;

	/* Replace current image */
	if ( ( rc = image_replace ( extracted ) ) != 0 )
		goto err_replace;

	/* Return to allow replacement image to be executed */
	return 0;

 err_replace:
 err_set_cmdline:
	unregister_image ( extracted );
 err_extract:
	return rc;
}

/* Drag in objects via image_extract() */
REQUIRING_SYMBOL ( image_extract );

/* Drag in archive image formats */
REQUIRE_OBJECT ( config_archive );
