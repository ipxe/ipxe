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

/** Maximum length of a command line */
#define CMDLINE_MAX 128

/** An executable or loadable image */
struct image {
	/** Name */
	char name[16];
	/** List of registered images */
	struct list_head list;
	/** Flags */
	unsigned int flags;

	/** Command line to pass to image */
	char cmdline[CMDLINE_MAX];
	/** Raw file image */
	userptr_t data;
	/** Length of raw file image */
	size_t len;

	/** Image type, if known */
	struct image_type *type;
	/** Entry point */
	physaddr_t entry;
};

/** Image is loaded */
#define IMAGE_LOADED 0x0001

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
	 * Load the image into memory at the correct location as
	 * determined by the file format.
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

/**
 * Multiboot image probe priority
 *
 * Multiboot images are also valid executables in another format
 * (e.g. ELF), so we must perform the multiboot probe first.
 */
#define PROBE_MULTIBOOT	01

/**
 * Normal image probe priority
 */
#define PROBE_NORMAL 02

/**
 * PXE image probe priority
 *
 * PXE images have no signature checks, so will claim all image files.
 * They must therefore be tried last in the probe order list.
 */
#define PROBE_PXE 03

/** An executable or loadable image type */
#define __image_type( probe_order ) \
	 __table ( struct image_type, image_types, probe_order )

extern struct list_head images;

/** Iterate over all registered images */
#define for_each_image( image ) \
	list_for_each_entry ( (image), &images, list )

extern int register_image ( struct image *image );
extern void unregister_image ( struct image *image );
extern void promote_image ( struct image *image );
struct image * find_image ( const char *name );
extern int image_load ( struct image *image );
extern int image_autoload ( struct image *image );
extern int image_exec ( struct image *image );

#endif /* _GPXE_IMAGE_H */
