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
#include <stdio.h>
#include <errno.h>
#include <gpxe/netdevice.h>
#include <gpxe/dhcp.h>
#include <gpxe/monojob.h>
#include <gpxe/process.h>
#include <usr/ifmgmt.h>
#include <usr/dhcpmgmt.h>

/** @file
 *
 * DHCP management
 *
 */

static int dhcp_success ( struct net_device *netdev __unused,
			  struct dhcp_option_block *options ) {
	DBGC ( options, "DHCP client registering options %p\n", options );
	register_dhcp_options ( options );
	return 0;
}

int dhcp ( struct net_device *netdev ) {
	struct dhcp_option_block *options;
	struct dhcp_option_block *tmp;
	int rc;

	/* Check we can open the interface first */
	if ( ( rc = ifopen ( netdev ) ) != 0 )
		return rc;

	/* Unregister any option blocks acquired via DHCP */
	list_for_each_entry_safe ( options, tmp, &dhcp_option_blocks, list ) {
		/* Skip static option blocks (e.g. from NVS) */
		if ( find_dhcp_option ( options, DHCP_MESSAGE_TYPE ) ) {
			DBGC ( options, "DHCP client unregistering options "
			       "%p\n", options );
			unregister_dhcp_options ( options );
		}
	}

	/* Perform DHCP */
	printf ( "DHCP (%s %s)", netdev->name, netdev_hwaddr ( netdev ) );
	if ( ( rc = start_dhcp ( &monojob, netdev, dhcp_success ) ) == 0 )
		rc = monojob_wait ( "" );

	return rc;
}
