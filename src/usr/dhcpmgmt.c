/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <byteswap.h>
#include <vsprintf.h>
#include <gpxe/in.h>
#include <gpxe/dhcp.h>
#include <gpxe/async.h>
#include <gpxe/netdevice.h>
#include <usr/ifmgmt.h>
#include <usr/dhcpmgmt.h>

/** @file
 *
 * DHCP management
 *
 */

/**
 * Configure network device via DHCP
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
int dhcp ( struct net_device *netdev ) {
	static struct dhcp_option_block *dhcp_options = NULL;
	struct dhcp_session dhcp;
	struct in_addr address = { htonl ( 0 ) };
	struct in_addr netmask = { htonl ( 0 ) };
	struct in_addr gateway = { INADDR_NONE };
	struct async async;
	int rc;

	/* Check we can open the interface first */
	if ( ( rc = ifopen ( netdev ) ) != 0 )
		return rc;

	/* Free up any previously-acquired options */
	if ( dhcp_options ) {
		unregister_dhcp_options ( dhcp_options );
		free_dhcp_options ( dhcp_options );
		dhcp_options = NULL;
	}

	/* Clear any existing routing table entry */
	del_ipv4_address ( netdev );

	/* Issue DHCP request */
	printf ( "DHCP (%s %s)...", netdev->name, netdev_hwaddr ( netdev ) );
	memset ( &dhcp, 0, sizeof ( dhcp ) );
	dhcp.netdev = netdev;
	async_init_orphan ( &async );
	if ( ( rc = start_dhcp ( &dhcp, &async ) ) != 0 ) {
		printf ( "could not start (%s)\n", strerror ( rc ) );
		return rc;
	}
	async_wait ( &async, &rc, 1 );	
	if ( rc != 0 ) {
		printf ( "failed (%s)\n", strerror ( rc ) );
		return rc;
	}
	printf ( "done\n" );

	/* Store and register options */
	dhcp_options = dhcp.options;
	register_dhcp_options ( dhcp_options );

	/* Retrieve IP address configuration */
	find_dhcp_ipv4_option ( dhcp_options, DHCP_EB_YIADDR, &address );
	find_dhcp_ipv4_option ( dhcp_options, DHCP_SUBNET_MASK, &netmask );
	find_dhcp_ipv4_option ( dhcp_options, DHCP_ROUTERS, &gateway );

	/* Set up new IP address configuration */
	if ( ( rc = add_ipv4_address ( netdev, address, netmask,
				       gateway ) ) != 0 ) {
		printf ( "Could not configure %s with DHCP results: %s\n",
			 netdev->name, strerror ( rc ) );
		return rc;
	}

	return 0;
}
