#ifndef _IPXE_GZIP_H
#define _IPXE_GZIP_H

/** @file
 *
 * gzip compressed images
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/image.h>

/** gzip header */
struct gzip_header {
	/** Magic ID */
	uint16_t magic;
	/** Compression method */
	uint8_t method;
	/** Flags */
	uint8_t flags;
	/** Modification time */
	uint32_t mtime;
	/** Extra flags */
	uint8_t extra;
	/** Operating system */
	uint8_t os;
} __attribute__ (( packed ));

/** Magic ID */
#define GZIP_MAGIC 0x1f8b

/** Compression method */
#define GZIP_METHOD_DEFLATE 0x08

/** CRC header is present */
#define GZIP_FL_HCRC 0x02

/** Extra header is present */
#define GZIP_FL_EXTRA 0x04

/** File name is present */
#define GZIP_FL_NAME 0x08

/** File comment is present */
#define GZIP_FL_COMMENT 0x10

/** gzip extra header */
struct gzip_extra_header {
	/** Extra header length (excluding this field) */
	uint16_t len;
} __attribute__ (( packed ));

/** gzip CRC header */
struct gzip_crc_header {
	/** CRC-16 */
	uint16_t crc;
} __attribute__ (( packed ));

/** gzip footer */
struct gzip_footer {
	/** CRC-32 */
	uint32_t crc;
	/** Uncompressed size (modulo 2^32) */
	uint32_t len;
} __attribute__ (( packed ));

extern struct image_type gzip_image_type __image_type ( PROBE_NORMAL );

#endif /* _IPXE_GZIP_H */
