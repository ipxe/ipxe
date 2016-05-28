#ifndef _APPLE_NET_BOOT_H_
#define _APPLE_NET_BOOT_H_

#define APPLE_NET_BOOT_PROTOCOL_GUID \
  { \
    0x78ee99fb, 0x6a5e, 0x4186, {0x97, 0xde, 0xcd, 0x0a, 0xba, 0x34, 0x5a, 0x74} \
  }

typedef struct _APPLE_NET_BOOT_PROTOCOL APPLE_NET_BOOT_PROTOCOL;

/**
  Get the DHCP packet obtained by the firmware during NetBoot.

  @param[in]    This            A pointer to th APPLE_NET_BOOT_PROTOCOL
                                instance.
  @param[inout] BufferSize      A pointer to the size of the buffer in bytes.
  @param[out]   DataBuffer      The memory buffer to copy the packet to. If it
                                is NULL, then the size of the packet is returned
                                in BufferSize.

  @retval EFI_SUCCESS           The packet was copied.
  @retval EFI_BUFFER_TOO_SMALL  The BufferSize is too small to read the current
                                packet. BufferSize has been updated with
                                the size needed to complete the request.
**/

typedef
EFI_STATUS
(EFIAPI* GET_DHCP_RESPONSE) (
  APPLE_NET_BOOT_PROTOCOL  *This,
  UINTN                    *BufferSize,
  VOID                     *DataBuffer
  );

/**
  Get the DHCP packet obtained by the firmware during NetBoot.

  @param[in]    This            A pointer to th APPLE_NET_BOOT_PROTOCOL
                                instance.
  @param[inout] BufferSize      A pointer to the size of the buffer in bytes.
  @param[out]   DataBuffer      The memory buffer to copy the packet to. If it
                                is NULL, then the size of the packet is returned
                                in BufferSize.

  @retval EFI_SUCCESS           The packet was copied.
  @retval EFI_BUFFER_TOO_SMALL  The BufferSize is too small to read the current
                                packet. BufferSize has been updated with
                                the size needed to complete the request.
**/

typedef
EFI_STATUS
(EFIAPI* GET_BSDP_RESPONSE) (
  APPLE_NET_BOOT_PROTOCOL  *This,
  UINTN                    *BufferSize,
  VOID                     *DataBuffer
  );

struct _APPLE_NET_BOOT_PROTOCOL
{
  GET_DHCP_RESPONSE        GetDhcpResponse;
  GET_BSDP_RESPONSE        GetBsdpResponse;
};

extern EFI_GUID gAppleNetBootProtocolGuid;

#endif
