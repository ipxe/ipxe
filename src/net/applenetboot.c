#include <ipxe/efi/efi.h>
#include <ipxe/settings.h>
#include <ipxe/applenetboot.h>
#include <stdlib.h>
#include <string.h>

static int applenetbooted_fetch ( void *data, size_t len ) {
	uint32_t result = htonl(get_apple_netbooted());
	memcpy(data, &result, len);
	return sizeof(uint32_t);
}

static int applenetboot_packetsize_fetch( void *data, size_t len ) {
	void ** dhcpResponse = NULL;
	UINTN size;
	get_apple_dhcp_packet(dhcpResponse, &size);
	free(dhcpResponse);
	size = htonl(size);
	memcpy(data, &size, len);
	return sizeof(uint32_t);
}

const struct setting applenetbooted_setting __setting ( SETTING_MISC, applenetbooted ) = {
	.name = "applenetbooted",
	.description = "Apple Netbooted",
	.type = &setting_type_uint32,
	.scope = &builtin_scope,
};

struct builtin_setting applenetbooted_builtin_setting __builtin_setting = {
	.setting = &applenetbooted_setting,
	.fetch = applenetbooted_fetch,
};

const struct setting applenetboot_packetsize_setting __setting ( SETTING_MISC, applenetboot_packetsize ) = {
	.name = "applenetboot_packetsize",
	.description = "Apple Netboot DHCP packet size",
	.type = &setting_type_uint32,
	.scope = &builtin_scope,
};

struct builtin_setting applenetboot_packetsize_builtin_setting __builtin_setting = {
	.setting = &applenetboot_packetsize_setting,
	.fetch = applenetboot_packetsize_fetch,
};
