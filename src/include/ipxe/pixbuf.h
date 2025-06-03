#ifndef _IPXE_PIXBUF_H
#define _IPXE_PIXBUF_H

/** @file
 *
 * Pixel buffer
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stddef.h>
#include <ipxe/refcnt.h>

/** A pixel buffer */
struct pixel_buffer {
	/** Reference count */
	struct refcnt refcnt;
	/** Width */
	unsigned int width;
	/** Height */
	unsigned int height;
	/** 32-bit (8:8:8:8) xRGB pixel data, in host-endian order */
	uint32_t *data;
	/** Total number of pixels */
	unsigned int pixels;
	/** Total length */
	size_t len;
};

/**
 * Get reference to pixel buffer
 *
 * @v pixbuf		Pixel buffer
 * @ret pixbuf		Pixel buffer
 */
static inline __attribute__ (( always_inline )) struct pixel_buffer *
pixbuf_get ( struct pixel_buffer *pixbuf ) {
	ref_get ( &pixbuf->refcnt );
	return pixbuf;
}

/**
 * Drop reference to pixel buffer
 *
 * @v pixbuf		Pixel buffer
 */
static inline __attribute__ (( always_inline )) void
pixbuf_put ( struct pixel_buffer *pixbuf ) {
	ref_put ( &pixbuf->refcnt );
}

/**
 * Get pixel
 *
 * @v pixbuf		Pixel buffer
 * @v x			X position
 * @v y			Y position
 * @ret pixel		Pixel
 */
static inline __attribute__ (( always_inline )) uint32_t *
pixbuf_pixel ( struct pixel_buffer *pixbuf, unsigned int x, unsigned int y ) {
	unsigned int index;

	index = ( ( y * pixbuf->width ) + x );
	return &pixbuf->data[index];
}

extern struct pixel_buffer * alloc_pixbuf ( unsigned int width,
					    unsigned int height );

#endif /* _IPXE_PIXBUF_H */
