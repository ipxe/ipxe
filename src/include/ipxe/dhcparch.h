#ifndef _IPXE_DHCPARCH_H
#define _IPXE_DHCPARCH_H

/** @file
 *
 * DHCP client architecture definitions
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/* Include platform-specific client architecture definitions */
#define PLATFORM_DHCPARCH(_platform) <ipxe/_platform/dhcparch.h>
#include PLATFORM_DHCPARCH(PLATFORM)

#endif /* _IPXE_DHCPARCH_H */
