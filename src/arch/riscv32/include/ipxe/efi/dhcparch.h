#ifndef _IPXE_EFI_DHCPARCH_H
#define _IPXE_EFI_DHCPARCH_H

/** @file
 *
 * DHCP client architecture definitions
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/dhcp.h>

/** DHCP client architecture */
#define DHCP_ARCH_CLIENT_ARCHITECTURE DHCP_CLIENT_ARCHITECTURE_RISCV32

/** DHCP client network device interface */
#define DHCP_ARCH_CLIENT_NDI 1 /* UNDI */ , 3, 10 /* v3.10 */

#endif /* _IPXE_EFI_DHCPARCH_H */
