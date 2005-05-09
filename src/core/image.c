#include "buffer.h"
#include "image.h"

static struct image images_start[0] __image_start;
static struct image images_end[0] __image_end;

/*
 * Identify the image format
 *
 */
static struct image * identify_image ( struct buffer *buffer ) {
	struct image_header header;
	int header_len = sizeof ( header );
	off_t len;
	struct image *image;
	
	/* Copy first (up to) 512 bytes of image to easily-accessible
	 * buffer.
	 */
	len = buffer->fill;
	copy_from_phys ( &header, buffer->start,
			 len < header_len ? len : header_len );
	
	for ( image = images_start ; image < images_end ; image++ ) {
		if ( image->probe ( &header, len ) )
			return image;
	}
	
	return NULL;
}

/*
 * Boot a loaded image
 *
 */
int boot_image ( struct buffer *buffer ) {
	struct image *image;

	image = identify_image ( buffer );
	if ( ! image ) {
		DBG ( "IMAGE could not identify image format\n" );
		return 0;
	}

	DBG ( "IMAGE found %s image (length %d)\n",
	      image->name, buffer->fill );

	return image->boot ( buffer->start, buffer->fill );
}
