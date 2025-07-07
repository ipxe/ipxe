#ifndef _IPXE_ZICBOM_H
#define _IPXE_ZICBOM_H

/** @file
 *
 * Cache-block management operations (Zicbom)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/iobuf.h>

extern void cache_clean ( struct io_buffer *iobuf );
extern void cache_invalidate ( struct io_buffer *iobuf );

#endif /* _IPXE_ZICBOM_H */
