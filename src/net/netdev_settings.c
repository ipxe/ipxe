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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <string.h>
#include <errno.h>
#include <gpxe/dhcp.h>
#include <gpxe/settings.h>
#include <gpxe/netdevice.h>

/** @file
 *
 * Network device configuration settings
 *
 */

/**
 * Store value of network device setting
 *
 * @v settings		Settings block
 * @v tag		Setting tag number
 * @v data		Setting data, or NULL to clear setting
 * @v len		Length of setting data
 * @ret rc		Return status code
 */
static int netdev_store ( struct settings *settings, unsigned int tag,
			  const void *data, size_t len ) {
	struct net_device *netdev =
		container_of ( settings, struct net_device, settings );

	switch ( tag ) {
	case DHCP_EB_MAC:
		if ( len != netdev->ll_protocol->ll_addr_len )
			return -EINVAL;
		memcpy ( netdev->ll_addr, data, len );
		return 0;
	default :
		return simple_settings_store ( settings, tag, data, len );
	}
}

/**
 * Fetch value of network device setting
 *
 * @v settings		Settings block
 * @v tag		Setting tag number
 * @v data		Setting data, or NULL to clear setting
 * @v len		Length of setting data
 * @ret rc		Return status code
 */
static int netdev_fetch ( struct settings *settings, unsigned int tag,
			  void *data, size_t len ) {
	struct net_device *netdev =
		container_of ( settings, struct net_device, settings );

	switch ( tag ) {
	case DHCP_EB_MAC:
		if ( len > netdev->ll_protocol->ll_addr_len )
			len = netdev->ll_protocol->ll_addr_len;
		memcpy ( data, netdev->ll_addr, len );
		return netdev->ll_protocol->ll_addr_len;
	default :
		return simple_settings_fetch ( settings, tag, data, len );
	}
}

/** Network device configuration settings operations */
struct settings_operations netdev_settings_operations = {
	.store = netdev_store,
	.fetch = netdev_fetch,
};

/** Network device named settings */
struct named_setting netdev_named_settings[] __named_setting = {
	{
		.name = "mac",
		.description = "MAC address",
		.tag = DHCP_EB_MAC,
		.type = &setting_type_hex,
	},
};
