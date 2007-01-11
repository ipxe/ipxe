#include "dev.h"
#include <gpxe/buffer.h>
#include "image.h"
#include <console.h>

static struct image images[0] __image_start;
static struct image images_end[0] __image_end;

/*
 * Print all images
 *
 */
void print_images ( void ) {
	struct image *image;

	for ( image = images ; image < images_end ; image++ ) {
		printf ( "%s ", image->name );
	}
}

#if 0

/*
 * Identify the image format
 *
 */
static struct image * identify_image ( physaddr_t start, physaddr_t len,
				       void **context ) {
	struct image *image;
	
	for ( image = images ; image < images_end ; image++ ) {
		if ( image->probe ( start, len, context ) )
			return image;
	}
	
	return NULL;
}

/*
 * Load an image into memory at a location determined by the image
 * format
 *
 */
int autoload ( struct dev *dev, struct image **image, void **context ) {
	struct buffer buffer;
	int rc = 0;

	/* Prepare the load buffer */
	if ( ! init_load_buffer ( &buffer ) ) {
		DBG ( "IMAGE could not initialise load buffer\n" );
		goto out;
	}

	/* Load the image into the load buffer */
	if ( ! load ( dev, &buffer ) ) {
		DBG ( "IMAGE could not load image\n" );
		goto out_free;
	}

	/* Shrink the load buffer */
	trim_load_buffer ( &buffer );

	/* Identify the image type */
	*image = identify_image ( buffer.start, buffer.fill, context );
	if ( ! *image ) {
		DBG ( "IMAGE could not identify image type\n" );
		goto out_free;
	}

	/* Move the image into the target location */
	if ( ! (*image)->load ( buffer.start, buffer.fill, *context ) ) {
		DBG ( "IMAGE could not move to target location\n" );
		goto out_free;
	}

	/* Return success */
	rc = 1;

 out_free:
	/* Free the load buffer */
	done_load_buffer ( &buffer );
 out:
	return rc;
}

#endif
