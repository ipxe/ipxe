#ifndef _IPXE_BLOB_H
#define _IPXE_BLOB_H

/** @file
 *
 * Openable data blobs
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>
#include <ipxe/interface.h>

extern int blob_open ( struct interface *xfer, const void *data, size_t len );

#endif /* _IPXE_BLOB_H */
