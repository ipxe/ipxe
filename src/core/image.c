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

FILE_LICENCE ( GPL2_OR_LATER );

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <libgen.h>
#include <gpxe/list.h>
#include <gpxe/umalloc.h>
#include <gpxe/uri.h>
#include <gpxe/image.h>

/** @file
 *
 * Executable/loadable images
 *
 */

/** List of registered images */
struct list_head images = LIST_HEAD_INIT ( images );

/**
 * Free executable/loadable image
 *
 * @v refcnt		Reference counter
 */
static void free_image ( struct refcnt *refcnt ) {
	struct image *image = container_of ( refcnt, struct image, refcnt );

	uri_put ( image->uri );
	ufree ( image->data );
	image_put ( image->replacement );
	free ( image );
	DBGC ( image, "IMAGE %p freed\n", image );
}

/**
 * Allocate executable/loadable image
 *
 * @ret image		Executable/loadable image
 */
struct image * alloc_image ( void ) {
	struct image *image;

	image = zalloc ( sizeof ( *image ) );
	if ( image ) {
		image->refcnt.free = free_image;
	}
	return image;
}

/**
 * Set image URI
 *
 * @v image		Image
 * @v URI		New image URI
 * @ret rc		Return status code
 *
 * If no name is set, the name will be updated to the base name of the
 * URI path (if any).
 */
int image_set_uri ( struct image *image, struct uri *uri ) {
	const char *path = uri->path;

	/* Replace URI reference */
	uri_put ( image->uri );
	image->uri = uri_get ( uri );

	/* Set name if none already specified */
	if ( path && ( ! image->name[0] ) )
		image_set_name ( image, basename ( ( char * ) path ) );

	return 0;
}

/**
 * Set image command line
 *
 * @v image		Image
 * @v cmdline		New image command line
 * @ret rc		Return status code
 */
int image_set_cmdline ( struct image *image, const char *cmdline ) {
	free ( image->cmdline );
	image->cmdline = strdup ( cmdline );
	if ( ! image->cmdline )
		return -ENOMEM;
	return 0;
}

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
	image_get ( image );
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
	DBGC ( image, "IMAGE %p unregistered\n", image );
	list_del ( &image->list );
	image_put ( image );
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
	for_each_table_entry ( type, IMAGE_TYPES ) {
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
	struct image *replacement;
	struct uri *old_cwuri;
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

	/* Switch current working directory to be that of the image itself */
	old_cwuri = uri_get ( cwuri );
	churi ( image->uri );

	/* Take out a temporary reference to the image.  This allows
	 * the image to unregister itself if necessary, without
	 * automatically freeing itself.
	 */
	image_get ( image );

	/* Try executing the image */
	if ( ( rc = image->type->exec ( image ) ) != 0 ) {
		DBGC ( image, "IMAGE %p could not execute: %s\n",
		       image, strerror ( rc ) );
		/* Do not return yet; we still have clean-up to do */
	}

	/* Pick up replacement image before we drop the original
	 * image's temporary reference.
	 */
	replacement = image->replacement;

	/* Drop temporary reference to the original image */
	image_put ( image );

	/* Reset current working directory */
	churi ( old_cwuri );
	uri_put ( old_cwuri );

	/* Tail-recurse into replacement image, if one exists */
	if ( replacement ) {
		DBGC ( image, "IMAGE %p replacing self with IMAGE %p\n",
		       image, replacement );
		if ( ( rc = image_exec ( replacement ) ) != 0 )
			return rc;
	}

	return rc;
}

/**
 * Register and autoload an image
 *
 * @v image		Image
 * @ret rc		Return status code
 */
int register_and_autoload_image ( struct image *image ) {
	int rc;

	if ( ( rc = register_image ( image ) ) != 0 )
		return rc;

	if ( ( rc = image_autoload ( image ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Register and autoexec an image
 *
 * @v image		Image
 * @ret rc		Return status code
 */
int register_and_autoexec_image ( struct image *image ) {
	int rc;

	if ( ( rc = register_and_autoload_image ( image ) ) != 0 )
		return rc;

	if ( ( rc = image_exec ( image ) ) != 0 )
		return rc;

	return 0;
}
