/*
 * Copyright (C) 2008 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/dhcp.h>
#include <ipxe/dhcpopts.h>
#include <ipxe/settings.h>
#include <ipxe/device.h>
#include <ipxe/netdevice.h>

/** @file
 *
 * Network device configuration settings
 *
 */

/** Network device named settings */
struct setting mac_setting __setting ( SETTING_NETDEV ) = {
	.name = "mac",
	.description = "MAC address",
	.type = &setting_type_hex,
};
struct setting busid_setting __setting ( SETTING_NETDEV ) = {
	.name = "busid",
	.description = "Bus ID",
	.type = &setting_type_hex,
};
struct setting chip_setting __setting ( SETTING_NETDEV ) = {
	.name = "chip",
	.description = "Chip",
	.type = &setting_type_string,
};

/**
 * Store MAC address setting
 *
 * @v netdev		Network device
 * @v data		Setting data, or NULL to clear setting
 * @v len		Length of setting data
 * @ret rc		Return status code
 */
static int netdev_store_mac ( struct net_device *netdev,
			      const void *data, size_t len ) {
	struct ll_protocol *ll_protocol = netdev->ll_protocol;

	/* Record new MAC address */
	if ( data ) {
		if ( len != netdev->ll_protocol->ll_addr_len )
			return -EINVAL;
		memcpy ( netdev->ll_addr, data, len );
	} else {
		/* Reset MAC address if clearing setting */
		ll_protocol->init_addr ( netdev->hw_addr, netdev->ll_addr );
	}

	return 0;
}

/**
 * Fetch MAC address setting
 *
 * @v netdev		Network device
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int netdev_fetch_mac ( struct net_device *netdev, void *data,
			      size_t len ) {

	if ( len > netdev->ll_protocol->ll_addr_len )
		len = netdev->ll_protocol->ll_addr_len;
	memcpy ( data, netdev->ll_addr, len );
	return netdev->ll_protocol->ll_addr_len;
}

/**
 * Fetch bus ID setting
 *
 * @v netdev		Network device
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int netdev_fetch_busid ( struct net_device *netdev, void *data,
				size_t len ) {
	struct device_description *desc = &netdev->dev->desc;
	struct dhcp_netdev_desc dhcp_desc;

	dhcp_desc.type = desc->bus_type;
	dhcp_desc.vendor = htons ( desc->vendor );
	dhcp_desc.device = htons ( desc->device );
	if ( len > sizeof ( dhcp_desc ) )
		len = sizeof ( dhcp_desc );
	memcpy ( data, &dhcp_desc, len );
	return sizeof ( dhcp_desc );
}

/**
 * Fetch chip setting
 *
 * @v netdev		Network device
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int netdev_fetch_chip ( struct net_device *netdev, void *data,
			       size_t len ) {
	const char *chip = netdev->dev->driver_name;

	strncpy ( data, chip, len );
	return strlen ( chip );
}

/** A network device setting operation */
struct netdev_setting_operation {
	/** Setting */
	struct setting *setting;
	/** Store setting (or NULL if not supported)
	 *
	 * @v netdev		Network device
	 * @v data		Setting data, or NULL to clear setting
	 * @v len		Length of setting data
	 * @ret rc		Return status code
	 */
	int ( * store ) ( struct net_device *netdev, const void *data,
			  size_t len );
	/** Fetch setting
	 *
	 * @v netdev		Network device
	 * @v data		Buffer to fill with setting data
	 * @v len		Length of buffer
	 * @ret len		Length of setting data, or negative error
	 */
	int ( * fetch ) ( struct net_device *netdev, void *data, size_t len );
};

/** Network device settings */
static struct netdev_setting_operation netdev_setting_operations[] = {
	{ &mac_setting, netdev_store_mac, netdev_fetch_mac },
	{ &busid_setting, NULL, netdev_fetch_busid },
	{ &chip_setting, NULL, netdev_fetch_chip },
};

/**
 * Store value of network device setting
 *
 * @v settings		Settings block
 * @v setting		Setting to store
 * @v data		Setting data, or NULL to clear setting
 * @v len		Length of setting data
 * @ret rc		Return status code
 */
static int netdev_store ( struct settings *settings, struct setting *setting,
			  const void *data, size_t len ) {
	struct net_device *netdev = container_of ( settings, struct net_device,
						   settings.settings );
	struct netdev_setting_operation *op;
	unsigned int i;

	/* Handle network device-specific settings */
	for ( i = 0 ; i < ( sizeof ( netdev_setting_operations ) /
			    sizeof ( netdev_setting_operations[0] ) ) ; i++ ) {
		op = &netdev_setting_operations[i];
		if ( setting_cmp ( setting, op->setting ) == 0 ) {
			if ( op->store ) {
				return op->store ( netdev, data, len );
			} else {
				return -ENOTSUP;
			}
		}
	}

	return generic_settings_store ( settings, setting, data, len );
}

/**
 * Fetch value of network device setting
 *
 * @v settings		Settings block
 * @v setting		Setting to fetch
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
static int netdev_fetch ( struct settings *settings, struct setting *setting,
			  void *data, size_t len ) {
	struct net_device *netdev = container_of ( settings, struct net_device,
						   settings.settings );
	struct netdev_setting_operation *op;
	unsigned int i;

	/* Handle network device-specific settings */
	for ( i = 0 ; i < ( sizeof ( netdev_setting_operations ) /
			    sizeof ( netdev_setting_operations[0] ) ) ; i++ ) {
		op = &netdev_setting_operations[i];
		if ( setting_cmp ( setting, op->setting ) == 0 )
			return op->fetch ( netdev, data, len );
	}

	return generic_settings_fetch ( settings, setting, data, len );
}

/**
 * Clear network device settings
 *
 * @v settings		Settings block
 */
static void netdev_clear ( struct settings *settings ) {
	generic_settings_clear ( settings );
}

/** Network device configuration settings operations */
struct settings_operations netdev_settings_operations = {
	.store = netdev_store,
	.fetch = netdev_fetch,
	.clear = netdev_clear,
};
