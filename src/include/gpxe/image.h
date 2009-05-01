#ifndef _GPXE_IMAGE_H
#define _GPXE_IMAGE_H

/**
 * @file
 *
 * Executable/loadable images
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <gpxe/tables.h>
#include <gpxe/list.h>
#include <gpxe/uaccess.h>
#include <gpxe/refcnt.h>

struct uri;
struct image_type;

/** An executable or loadable image */
struct image {
	/** Reference count */
	struct refcnt refcnt;

	/** List of registered images */
	struct list_head list;

	/** URI of image */
	struct uri *uri;
	/** Name */
	char name[16];
	/** Flags */
	unsigned int flags;

	/** Command line to pass to image */
	char *cmdline;
	/** Raw file image */
	userptr_t data;
	/** Length of raw file image */
	size_t len;

	/** Image type, if known */
	struct image_type *type;
	/** Image type private data */
	union {
		physaddr_t phys;
		userptr_t user;
		unsigned long ul;
	} priv;

	/** Replacement image
	 *
	 * An image wishing to replace itself with another image (in a
	 * style similar to a Unix exec() call) should return from its
	 * exec() method with the replacement image set to point to
	 * the new image.  The new image must already be in a suitable
	 * state for execution (i.e. loaded).
	 *
	 * If an image unregisters itself as a result of being
	 * executed, it must make sure that its replacement image (if
	 * any) is registered, otherwise the replacement is likely to
	 * be freed before it can be executed.
	 */
	struct image *replacement;
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
	 *
	 * Note that the image may be invalidated by the act of
	 * execution, i.e. an image is allowed to choose to unregister
	 * (and so potentially free) itself.
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

/** Executable or loadable image type table */
#define IMAGE_TYPES __table ( struct image_type, "image_types" )

/** An executable or loadable image type */
#define __image_type( probe_order ) __table_entry ( IMAGE_TYPES, probe_order )

extern struct list_head images;

/** Iterate over all registered images */
#define for_each_image( image ) \
	list_for_each_entry ( (image), &images, list )

/**
 * Test for existence of images
 *
 * @ret existence	Some images exist
 */
static inline int have_images ( void ) {
	return ( ! list_empty ( &images ) );
}

extern struct image * alloc_image ( void );
extern int image_set_uri ( struct image *image, struct uri *uri );
extern int image_set_cmdline ( struct image *image, const char *cmdline );
extern int register_image ( struct image *image );
extern void unregister_image ( struct image *image );
extern void promote_image ( struct image *image );
struct image * find_image ( const char *name );
extern int image_load ( struct image *image );
extern int image_autoload ( struct image *image );
extern int image_exec ( struct image *image );
extern int register_and_autoload_image ( struct image *image );
extern int register_and_autoexec_image ( struct image *image );

/**
 * Increment reference count on an image
 *
 * @v image		Image
 * @ret image		Image
 */
static inline struct image * image_get ( struct image *image ) {
	ref_get ( &image->refcnt );
	return image;
}

/**
 * Decrement reference count on an image
 *
 * @v image		Image
 */
static inline void image_put ( struct image *image ) {
	ref_put ( &image->refcnt );
}

/**
 * Set image name
 *
 * @v image		Image
 * @v name		New image name
 * @ret rc		Return status code
 */
static inline int image_set_name ( struct image *image, const char *name ) {
	strncpy ( image->name, name, ( sizeof ( image->name ) - 1 ) );
	return 0;
}

#endif /* _GPXE_IMAGE_H */
