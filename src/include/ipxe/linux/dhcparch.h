#ifndef _IPXE_LINUX_DHCPARCH_H
#define _IPXE_LINUX_DHCPARCH_H

/** @file
 *
 * DHCP client architecture definitions
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/*
 * There are no specification-defined values for DHCP architecture for
 * PXE clients running as Linux userspace applications.  Pretend to be
 * the equivalent EFI client.
 *
 */
#include <ipxe/efi/dhcparch.h>

#endif /* _IPXE_LINUX_DHCPARCH_H */
