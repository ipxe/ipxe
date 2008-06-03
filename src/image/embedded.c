/** @file
 *
 * Take a possible embedded image and put it in a struct image
 * data structure.
 */

#include <stdio.h>
#include <gpxe/image.h>
#include <gpxe/malloc.h>
#include <gpxe/uaccess.h>
#include <gpxe/umalloc.h>
#include <gpxe/embedded.h>

extern char _embedded_image_start[], _embedded_image_end[];

struct image *embedded_image(void)
{
	static int reclaimed = 0;
	struct image *image;
	size_t eisize = _embedded_image_end - _embedded_image_start;

	if ( !eisize )
		return NULL;	/* No embedded image */

	if ( reclaimed )
		return NULL;	/* Already reclaimed */

	printf("Embedded image: %d bytes at %p\n",
	       eisize, _embedded_image_start);

	image = alloc_image();
	if (!image)
		return NULL;

	image->len     = eisize;
	image->data    = umalloc(eisize);
	if (image->data == UNULL) {
		image_put(image);
		return image = NULL;
	}
	copy_to_user(image->data, 0, _embedded_image_start, eisize);
	register_image(image);

	/* Reclaim embedded image memory */
	reclaimed = 1;
	mpopulate(_embedded_image_start, eisize);

	return image;
}

