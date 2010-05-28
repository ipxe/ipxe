#ifndef _IPXE_BASE64_H
#define _IPXE_BASE64_H

/** @file
 *
 * Base64 encoding
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>

/**
 * Calculate length of base64-encoded data
 *
 * @v raw_len		Raw data length
 * @ret encoded_len	Encoded string length (excluding NUL)
 */
static inline size_t base64_encoded_len ( size_t raw_len ) {
	return ( ( ( raw_len + 3 - 1 ) / 3 ) * 4 );
}

extern void base64_encode ( const uint8_t *raw, size_t len, char *encoded );

#endif /* _IPXE_BASE64_H */
