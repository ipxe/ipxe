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
#include <gpxe/netdevice.h>
#include <gpxe/device.h>
#include <usr/ifmgmt.h>

/** @file
 *
 * Network interface management
 *
 */

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
int ifopen ( struct net_device *netdev ) {
	int rc;

	if ( ( rc = netdev_open ( netdev ) ) != 0 ) {
		printf ( "Could not open %s: %s\n",
			 netdev->name, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
void ifclose ( struct net_device *netdev ) {
	netdev_close ( netdev );
}

/**
 * Print status of network device
 *
 * @v netdev		Network device
 */
void ifstat ( struct net_device *netdev ) {
	printf ( "%s: %s on %s (%s) TX:%d TXE:%d RX:%d RXE:%d\n",
		 netdev->name, netdev_hwaddr ( netdev ), netdev->dev->name,
		 ( ( netdev->state & NETDEV_OPEN ) ? "open" : "closed" ),
		 netdev->stats.tx_ok, netdev->stats.tx_err,
		 netdev->stats.rx_ok, netdev->stats.rx_err );
}
