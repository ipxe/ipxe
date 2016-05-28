#ifndef _IPXE_APPLENETBOOT_H
#define _IPXE_APPLENETBOOT_H

#include <ipxe/efi/efi.h>

extern int get_apple_netbooted();
extern int get_apple_dhcp_packet(void * buffer, UINTN * size);
extern int efi_applenetboot_install(EFI_HANDLE handle);

#endif
