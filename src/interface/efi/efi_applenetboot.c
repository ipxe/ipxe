#include <ipxe/applenetboot.h>
#include <ipxe/efi/Protocol/AppleNetBoot.h>
#include <stdlib.h>
#include <string.h>
#include <ipxe/fakedhcp.h>
#include <ipxe/netdevice.h>

static APPLE_NET_BOOT_PROTOCOL *applenetboot;
EFI_REQUEST_PROTOCOL ( APPLE_NET_BOOT_PROTOCOL, &applenetboot );

EFI_GUID gAppleNetBootProtocolGuid
        = APPLE_NET_BOOT_PROTOCOL_GUID;

int get_apple_netbooted() {
	return (applenetboot != NULL);
}

int get_apple_dhcp_packet(void * buffer, UINTN * size) {
	return applenetboot->GetDhcpResponse(applenetboot, size, buffer);
}

static EFI_STATUS EFIAPI
getdhcp(APPLE_NET_BOOT_PROTOCOL *This __unused, UINTN *BufferSize, VOID *DataBuffer)
{
	struct net_device *boot_netdev;
	const size_t max_len = 576;

	if (*BufferSize < max_len)
	{
		*BufferSize = max_len;
		return EFI_BUFFER_TOO_SMALL;
	}

	boot_netdev = last_opened_netdev();
	create_fakedhcpack(boot_netdev, DataBuffer, max_len);
	return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI
getbsdp(APPLE_NET_BOOT_PROTOCOL *This __unused, UINTN *BufferSize, VOID *DataBuffer)
{
	struct net_device *boot_netdev;
	const size_t max_len = 576;

	if (*BufferSize < max_len)
	{
		*BufferSize = max_len;
		return EFI_BUFFER_TOO_SMALL;
	}

	boot_netdev = last_opened_netdev();
	create_fakepxebsack(boot_netdev, DataBuffer, max_len);
	return EFI_SUCCESS;
}

static APPLE_NET_BOOT_PROTOCOL applenetboot2 = {
	.GetDhcpResponse = getdhcp,
	.GetBsdpResponse = getbsdp,
};

int efi_applenetboot_install(EFI_HANDLE handle)
{
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_STATUS efirc = EFI_SUCCESS;

	efirc = bs->InstallMultipleProtocolInterfaces (
	         &handle, &gAppleNetBootProtocolGuid, &applenetboot2, NULL);
	return efirc;
}
