#ifndef _IPXE_PCBIOS_DHCPARCH_H
#define _IPXE_PCBIOS_DHCPARCH_H

/** @file
 *
 * DHCP client architecture definitions
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/dhcp.h>

/** DHCP client architecture */
#define DHCP_ARCH_CLIENT_ARCHITECTURE DHCP_CLIENT_ARCHITECTURE_X86

/** DHCP client network device interface */
#define DHCP_ARCH_CLIENT_NDI 1 /* UNDI */ , 2, 1 /* v2.1 */

#endif /* _IPXE_PCBIOS_DHCPARCH_H */
