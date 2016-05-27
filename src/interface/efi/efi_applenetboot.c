#include <ipxe/applenetboot.h>
#include <ipxe/efi/Protocol/AppleNetBoot.h>
#include <stdlib.h>
#include <string.h>

static APPLE_NET_BOOT_PROTOCOL *applenetboot;
EFI_REQUEST_PROTOCOL ( APPLE_NET_BOOT_PROTOCOL, &applenetboot );

EFI_GUID gAppleNetBootProtocolGuid
        = APPLE_NET_BOOT_PROTOCOL_GUID;;

int get_apple_netbooted() {
	return (applenetboot != NULL);
}

int get_apple_dhcp_packet(void ** dhcpResponse, UINTN * size) {
	EFI_STATUS status;
	UINTN bufferSize = 0;

	status = applenetboot->GetDhcpResponse(applenetboot, &bufferSize, NULL);
	if(status != EFI_BUFFER_TOO_SMALL)
		return status;
	*size = bufferSize;

	dhcpResponse = realloc(dhcpResponse, bufferSize);
	status = applenetboot->GetDhcpResponse(applenetboot, &bufferSize, dhcpResponse);
	return status;

}

int efi_applenetboot_install(EFI_HANDLE handle)
{
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_STATUS efirc = EFI_SUCCESS;

	efirc = bs->InstallMultipleProtocolInterfaces (
	         &handle, &gAppleNetBootProtocolGuid, &applenetboot, NULL);
	return efirc;
}

