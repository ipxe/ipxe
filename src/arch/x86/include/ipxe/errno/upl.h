#ifndef _IPXE_ERRNO_UPL_H
#define _IPXE_ERRNO_UPL_H

/**
 * @file
 *
 * Universal Payload platform error codes
 *
 * We never need to return platform error codes ourselves, so we
 * arbitrarily choose to use the Linux error codes as platform error
 * codes.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/errno/linux.h>

#endif /* _IPXE_ERRNO_UPL_H */
