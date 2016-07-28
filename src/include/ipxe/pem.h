#ifndef _IPXE_PEM_H
#define _IPXE_PEM_H

/** @file
 *
 * PEM image format
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/image.h>

/** Pre-encapsulation boundary marker */
#define PEM_BEGIN "-----BEGIN"

/** Post-encapsulation boundary marker */
#define PEM_END "-----END"

extern struct image_type pem_image_type __image_type ( PROBE_NORMAL );

#endif /* _IPXE_PEM_H */
