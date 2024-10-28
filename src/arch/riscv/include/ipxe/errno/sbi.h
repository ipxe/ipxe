#ifndef _IPXE_ERRNO_SBI_H
#define _IPXE_ERRNO_SBI_H

/**
 * @file
 *
 * RISC-V SBI platform error codes
 *
 * We never need to return SBI error codes ourselves, so we
 * arbitrarily choose to use the Linux error codes as platform error
 * codes.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/errno/linux.h>

#endif /* _IPXE_ERRNO_SBI_H */
