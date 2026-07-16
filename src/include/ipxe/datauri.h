#ifndef _IPXE_DATAURI_H
#define _IPXE_DATAURI_H

/** @file
 *
 * Data URIs
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>
#include <string.h>
#include <ipxe/uri.h>

/**
 * Get maximum length of parsed data URI
 *
 * @v uri		Data URI
 * @ret max_len		Maximum length
 */
static inline size_t datauri_max_len ( struct uri *uri ) {

	/* Neither URI decoding nor base64 decoding can ever expand data */
	return ( uri->opaque ? strlen ( uri->opaque ) : 0 );
}

extern int datauri_parse ( struct uri *uri, void *buf );

#endif /* _IPXE_DATAURI_H */
