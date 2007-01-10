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

void autoboot ( void ) {
	struct net_device *netdev;
	int rc;

	for_each_netdev ( netdev ) {

		if ( ( rc = ifopen ( netdev ) ) != 0 )
			continue;

		ifstat ( netdev );

		test_dhcp ( netdev );
		
		ifclose ( netdev );
	}

	printf ( "No more network devices\n" );
}
