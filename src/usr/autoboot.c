/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <vsprintf.h>
#include <gpxe/netdevice.h>
#include <usr/ifmgmt.h>
#include <usr/autoboot.h>

/** @file
 *
 * Automatic booting
 *
 */

void test_dhcp ( struct net_device *netdev );

/**
 * Identify the boot network device
 *
 * @ret netdev		Boot network device
 */
static struct net_device * find_boot_netdev ( void ) {
	return NULL;
}

/**
 * Get the next network device to try
 *
 * @ret netdev		'Next' network device
 *
 * This function will cycle through all registered network devices in
 * order, returning NULL.
 *
 * This function should be safe against registration/deregistration of
 * net devices between calls to next_netdev().
 */
static struct net_device * next_netdev ( void ) {
	static struct net_device *last_netdev = NULL;
	struct net_device *netdev;

	for_each_netdev ( netdev ) {
		if ( ! last_netdev ) {
			last_netdev = netdev;
			return netdev;
		}
		if ( last_netdev == netdev )
			last_netdev = NULL;
	}

	last_netdev = NULL;
	return NULL;
}

/**
 * Boot from a network device
 *
 * @v netdev		Network device
 */
void netboot ( struct net_device *netdev ) {

	/* Open device and display device status */
	if ( ifopen ( netdev ) != 0 )
		return;
	ifstat ( netdev );

	test_dhcp ( netdev );
}

/**
 * Close all open net devices
 *
 * Called before a fresh boot attempt in order to free up memory.  We
 * don't just close the device immediately after the boot fails,
 * because there may still be TCP connections in the process of
 * closing.
 */
static void close_all_netdevs ( void ) {
	struct net_device *netdev;

	for_each_netdev ( netdev ) {
		ifclose ( netdev );
	}
}

/**
 * Boot the system
 */
void autoboot ( void ) {
	struct net_device *boot_netdev;
	struct net_device *netdev;

	/* If we have an identifable boot device, try that first */
	close_all_netdevs();
	if ( ( boot_netdev = find_boot_netdev() ) )
		netboot ( boot_netdev );

	/* If that fails, try booting from any of the other devices */
	for_each_netdev ( netdev ) {
		if ( netdev == boot_netdev )
			continue;
		close_all_netdevs();
		netboot ( netdev );
	}

	printf ( "No more network devices\n" );
}
