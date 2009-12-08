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

FILE_LICENCE ( GPL2_OR_LATER );

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <gpxe/netdevice.h>
#include <gpxe/dhcp.h>
#include <gpxe/monojob.h>
#include <gpxe/process.h>
#include <usr/ifmgmt.h>
#include <usr/dhcpmgmt.h>

#define LINK_WAIT_MS	15000

/** @file
 *
 * DHCP management
 *
 */

int dhcp ( struct net_device *netdev ) {
	uint8_t *chaddr;
	uint8_t hlen;
	uint16_t flags;
	int rc;

	/* Check we can open the interface first */
	if ( ( rc = ifopen ( netdev ) ) != 0 )
		return rc;

	/* Wait for link-up */
	if ( ( rc = iflinkwait ( netdev, LINK_WAIT_MS ) ) != 0 )
		return rc;

	/* Perform DHCP */
	chaddr = dhcp_chaddr ( netdev, &hlen, &flags );
	printf ( "DHCP (%s ", netdev->name );
	while ( hlen-- )
		printf ( "%02x%c", *(chaddr++), ( hlen ? ':' : ')' ) );

	if ( ( rc = start_dhcp ( &monojob, netdev ) ) == 0 ) {
		rc = monojob_wait ( "" );
	} else if ( rc > 0 ) {
		printf ( " using cached\n" );
		rc = 0;
	}

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
