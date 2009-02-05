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

int dhcp ( struct net_device *netdev ) {
	int rc;

	/* Check we can open the interface first */
	if ( ( rc = ifopen ( netdev ) ) != 0 )
		return rc;

	/* Perform DHCP */
	printf ( "DHCP (%s %s)", netdev->name, netdev_hwaddr ( netdev ) );
	if ( ( rc = start_dhcp ( &monojob, netdev ) ) == 0 )
		rc = monojob_wait ( "" );

	return rc;
}

int pxebs ( struct net_device *netdev, unsigned int pxe_type ) {
	int rc;

	/* Perform PXE Boot Server Discovery */
	printf ( "PXEBS (%s type %d)", netdev->name, pxe_type );
	if ( ( rc = start_pxebs ( &monojob, netdev, pxe_type ) ) == 0 )
		rc = monojob_wait ( "" );

	return rc;
}
