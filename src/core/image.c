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

	/* Create image name if it doesn't already have one */
	if ( ! image->name[0] ) {
		snprintf ( image->name, sizeof ( image->name ), "img%d",
			   imgindex++ );
	}

	/* Add to image list */
	list_add_tail ( &image->list, &images );
	DBGC ( image, "IMAGE %p at [%lx,%lx) registered as %s\n",
	       image, user_to_phys ( image->data, 0 ),
	       user_to_phys ( image->data, image->len ), image->name );

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
 * Move image to start of list of registered images
 *
 * @v image		Executable/loadable image
 *
 * Move the image to the start of the image list.  This makes it
 * easier to keep track of which of the images marked as loaded is
 * likely to still be valid.
 */
void promote_image ( struct image *image ) {
	list_del ( &image->list );
	list_add ( &image->list, &images );
}

/**
 * Find image by name
 *
 * @v name		Image name
 * @ret image		Executable/loadable image, or NULL
 */
struct image * find_image ( const char *name ) {
	struct image *image;

	list_for_each_entry ( image, &images, list ) {
		if ( strcmp ( image->name, name ) == 0 )
			return image;
	}

	return NULL;
}

/**
 * Load executable/loadable image into memory
 *
 * @v image		Executable/loadable image
 * @v type		Executable/loadable image type
 * @ret rc		Return status code
 */
static int image_load_type ( struct image *image, struct image_type *type ) {
	int rc;

	/* Check image is actually loadable */
	if ( ! type->load )
		return -ENOEXEC;

	/* Try the image loader */
	if ( ( rc = type->load ( image ) ) != 0 ) {
		DBGC ( image, "IMAGE %p could not load as %s: %s\n",
		       image, type->name, strerror ( rc ) );
		return rc;
	}

	/* Flag as loaded */
	image->flags |= IMAGE_LOADED;
	return 0;
}

/**
 * Load executable/loadable image into memory
 *
 * @v image		Executable/loadable image
 * @ret rc		Return status code
 */
int image_load ( struct image *image ) {

	assert ( image->type != NULL );

	return image_load_type ( image, image->type );
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

	/* If image already has a type, use it */
	if ( image->type )
		return image_load ( image );

	/* Otherwise probe for a suitable type */
	for ( type = image_types ; type < image_types_end ; type++ ) {
		DBGC ( image, "IMAGE %p trying type %s\n", image, type->name );
		rc = image_load_type ( image, type );
		if ( image->type == NULL )
			continue;
		return rc;
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

	/* Image must be loaded first */
	if ( ! ( image->flags & IMAGE_LOADED ) ) {
		DBGC ( image, "IMAGE %p could not execute: not loaded\n",
		       image );
		return -ENOTTY;
	}

	assert ( image->type != NULL );

	/* Check that image is actually executable */
	if ( ! image->type->exec )
		return -ENOEXEC;

	/* Try executing the image */
	if ( ( rc = image->type->exec ( image ) ) != 0 ) {
		DBGC ( image, "IMAGE %p could not execute: %s\n",
		       image, strerror ( rc ) );
		return rc;
	}

	/* Well, some formats might return... */
	return 0;
}
