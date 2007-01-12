#ifndef _GPXE_UACCESS_H
#define _GPXE_UACCESS_H

/**
 * @file
 *
 * Access to external ("user") memory
 *
 * gPXE often needs to transfer data between internal and external
 * buffers.  On i386, the external buffers may require access via a
 * different segment, and the buffer address cannot be encoded into a
 * simple void * pointer.  The @c userptr_t type encapsulates the
 * information needed to identify an external buffer, and the
 * copy_to_user() and copy_from_user() functions provide methods for
 * transferring data between internal and external buffers.
 *
 * Note that userptr_t is an opaque type; in particular, performing
 * arithmetic upon a userptr_t is not allowed.
 *
 */

#include <bits/uaccess.h>

/** Equivalent of NULL for user pointers */
#define UNULL ( ( userptr_t ) 0 )

#endif /* _GPXE_UACCESS_H */
