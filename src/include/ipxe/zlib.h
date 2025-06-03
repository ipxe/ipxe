#ifndef _IPXE_ZLIB_H
#define _IPXE_ZLIB_H

/** @file
 *
 * zlib compressed images
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <byteswap.h>
#include <ipxe/image.h>
#include <ipxe/deflate.h>

/** zlib magic header */
union zlib_magic {
	/** Compression method and flags */
	uint8_t cmf;
	/** Check value */
	uint16_t check;
} __attribute__ (( packed ));

/**
 * Check that zlib magic header is valid
 *
 * @v magic		Magic header
 * @ret is_valid	Magic header is valid
 */
static inline int zlib_magic_is_valid ( const union zlib_magic *magic ) {

	/* Check magic value as per RFC 6713 */
	return ( ( ( magic->cmf & 0x8f ) == 0x08 ) &&
		 ( ( be16_to_cpu ( magic->check ) % 31 ) == 0 ) );
}

extern int zlib_deflate ( enum deflate_format format, const void *data,
			  size_t len, struct image *extracted );

extern struct image_type zlib_image_type __image_type ( PROBE_NORMAL );

#endif /* _IPXE_ZLIB_H */
