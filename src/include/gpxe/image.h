#ifndef _GPXE_IMAGE_H
#define _GPXE_IMAGE_H

/**
 * @file
 *
 * Executable/loadable image formats
 *
 */

#include <gpxe/tables.h>

/** An executable or loadable image */
struct image {
	/** Raw file image */
	userptr_t image;
	/** Length of raw file image */
	size_t len;

	/** Execute method
	 *
	 * Filled in by the image loader.  If NULL, then the image
	 * cannot be executed.
	 */
	int ( * execute ) ( struct image *image );
	/** Entry point */
	physaddr_t entry;
};

/** An executable or loadable image type */
struct image_type {
	/** Name of this image type */
	char *name;
	/** Load image into memory
	 *
	 * @v image		Executable/loadable image
	 * @ret rc		Return status code
	 *
	 * Load the image into memory.  The file image may be
	 * discarded after this call; the method must preserve any
	 * information it may require later (e.g. the execution
	 * address) within the @c image structure.
	 */
	int ( * load ) ( struct image *image );
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

#endif /* _GPXE_IMAGE_H */
