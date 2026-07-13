#ifndef _IPXE_MIME_H
#define _IPXE_MIME_H

/** @file
 *
 * MIME format
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/image.h>

/** A MIME attribute */
struct mime_attribute {
	/** Attribute value (not NUL-terminated) */
	const char *value;
	/** Length of value */
	size_t len;
};

/** MIME headers */
struct mime_headers {
	/** Content transfer encoding (not NUL-terminated) */
	const char *encoding;
	/** Content type (not NUL-terminated) */
	const char *type;
	/** Boundary separator */
	struct mime_attribute boundary;
	/** Length of headers (including empty line separator) */
	size_t len;
};

/** A recognised MIME header */
struct mime_header {
	/** Name (including terminating colon) */
	const char *name;
	/**
	 * Parse header value
	 *
	 * @v image		MIME image
	 * @v value		Header value
	 * @v eol		End of header line
	 * @v headers		MIME headers to update
	 */
	void ( * parse ) ( struct image *image, const char *value,
			   const char *eol, struct mime_headers *headers );
};

/** A MIME content transfer encoding */
struct mime_encoding {
	/** Name */
	const char *name;
	/**
	 * Decode entity
	 *
	 * @v image		MIME image
	 * @v headers		MIME headers
	 * @v decoded		Decoded image
	 * @ret rc		Return status code
	 */
	int ( * decode ) ( struct image *image,
			   const struct mime_headers *headers,
			   struct image *decoded );
};

/** A MIME type */
struct mime_type {
	/** Name */
	const char *name;
	/**
	 * Extract image
	 *
	 * @v mime		MIME image
	 * @v headers		MIME headers
	 * @v extracted		Extracted image
	 * @ret rc		Return status code
	 */
	int ( * extract ) ( struct image *image,
			    const struct mime_headers *headers,
			    struct image *extracted );
};

extern struct image_type mime_image_type __image_type ( PROBE_NORMAL );

#endif /* _IPXE_MIME_H */
