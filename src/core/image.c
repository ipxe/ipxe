/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <vsprintf.h>
#include <gpxe/list.h>
#include <gpxe/image.h>

/** @file
 *
 * Executable/loadable images
 *
 */

/** List of registered images */
struct list_head images = LIST_HEAD_INIT ( images );

/** List of image types */
static struct image_type image_types[0]
	__table_start ( struct image_type, image_types );
static struct image_type image_types_end[0]
	__table_end ( struct image_type, image_types );

/**
 * Register executable/loadable image
 *
 * @v image		Executable/loadable image
 * @ret rc		Return status code
 */
int register_image ( struct image *image ) {
	static unsigned int imgindex = 0;

	/* Create image name */
	snprintf ( image->name, sizeof ( image->name ), "img%d",
		   imgindex++ );

	/* Add to image list */
	list_add_tail ( &image->list, &images );
	DBGC ( image, "IMAGE %p registered as %s\n", image, image->name );

	return 0;
}

/**
 * Unregister executable/loadable image
 *
 * @v image		Executable/loadable image
 */
void unregister_image ( struct image *image ) {
	list_del ( &image->list );
	DBGC ( image, "IMAGE %p unregistered\n", image );
}

/**
 * Load executable/loadable image into memory
 *
 * @v image		Executable/loadable image
 * @ret rc		Return status code
 */
int image_load ( struct image *image ) {
	int rc;

	assert ( image->type != NULL );

	if ( ( rc = image->type->load ( image ) ) != 0 ) {
		DBGC ( image, "IMAGE %p could not load: %s\n",
		       image, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Autodetect image type and load executable/loadable image into memory
 *
 * @v image		Executable/loadable image
 * @ret rc		Return status code
 */
int image_autoload ( struct image *image ) {
	struct image_type *type;
	int rc;

	for ( type = image_types ; type < image_types_end ; type++ ) {
		rc = type->load ( image );
		if ( image->type == NULL )
			continue;
		if ( rc != 0 ) {
			DBGC ( image, "IMAGE %p (%s) could not load: %s\n",
			       image, image->type->name, strerror ( rc ) );
			return rc;
		}
		return 0;
	}

	DBGC ( image, "IMAGE %p format not recognised\n", image );
	return -ENOEXEC;
}

/**
 * Execute loaded image
 *
 * @v image		Loaded image
 * @ret rc		Return status code
 */
int image_exec ( struct image *image ) {
	int rc;

	assert ( image->type != NULL );

	if ( ( rc = image->type->exec ( image ) ) != 0 ) {
		DBGC ( image, "IMAGE %p could not execute: %s\n",
		       image, strerror ( rc ) );
		return rc;
	}

	/* Well, some formats might return... */
	return 0;
}
