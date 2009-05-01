#ifndef _GPXE_BASE64_H
#define _GPXE_BASE64_H

/** @file
 *
 * Base64 encoding
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>

/**
 * Calculate length of base64-encoded string
 *
 * @v raw_len		Raw string length (excluding NUL)
 * @ret encoded_len	Encoded string length (excluding NUL)
 */
static inline size_t base64_encoded_len ( size_t raw_len ) {
	return ( ( ( raw_len + 3 - 1 ) / 3 ) * 4 );
}

extern void base64_encode ( const char *raw, char *encoded );

#endif /* _GPXE_BASE64_H */
