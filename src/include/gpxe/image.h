#ifndef _GPXE_IMAGE_H
#define _GPXE_IMAGE_H

/**
 * @file
 *
 * Executable/loadable images
 *
 */

#include <gpxe/tables.h>
#include <gpxe/list.h>
#include <gpxe/uaccess.h>

struct image_type;

/** An executable or loadable image */
struct image {
	/** Name */
	char name[16];
	/** List of registered images */
	struct list_head list;

	/** Command line to pass to image */
	const char *cmdline;

	/** Raw file image */
	userptr_t data;
	/** Length of raw file image */
	size_t len;

	/** Entry point */
	physaddr_t entry;

	/** Image type, if known */
	struct image_type *type;
};

/** An executable or loadable image type */
struct image_type {
	/** Name of this image type */
	char *name;
	/**
	 * Load image into memory
	 *
	 * @v image		Executable/loadable image
	 * @ret rc		Return status code
	 *
	 * Load the image into memory.  The file image may be
	 * discarded after this call; the method must preserve any
	 * information it may require later (e.g. the execution
	 * address) within the @c image structure.
	 *
	 * If the file image is in the correct format, the method must
	 * update @c image->type to point to its own type (unless @c
	 * image->type is already set).  This allows the autoloading
	 * code to disambiguate between "this is not my image format"
	 * and "there is something wrong with this image".  In
	 * particular, setting @c image->type and then returning an
	 * error will cause image_autoload() to abort and return an
	 * error, rather than continuing to the next image type.
	 */
	int ( * load ) ( struct image *image );
	/**
	 * Execute loaded image
	 *
	 * @v image		Loaded image
	 * @ret rc		Return status code
	 */
	int ( * exec ) ( struct image *image );
};

/** An executable or loadable image type */
#define __image_type __table ( struct image_type, image_types, 01 )

/**
 * An unverifiable executable or loadable image type
 *
 * This should be used to mark image types for which there are no
 * signature or other checks that can be used to verify the validity
 * of the image (such as PXE images).  These will then be tried last
 * in the list of image types.
 */
#define __default_image_type __table ( struct image_type, image_types, 02 )

extern struct list_head images;

/** Iterate over all registered images */
#define for_each_image( image ) \
	list_for_each_entry ( (image), &images, list )

extern int register_image ( struct image *image );
extern void unregister_image ( struct image *image );
extern int image_load ( struct image *image );
extern int image_autoload ( struct image *image );
extern int image_exec ( struct image *image );

#endif /* _GPXE_IMAGE_H */
