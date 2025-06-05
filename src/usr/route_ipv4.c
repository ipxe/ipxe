/*
 * Copyright (C) 2013 Michael Brown <mbrown@fensystems.co.uk>.
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
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdio.h>
#include <ipxe/netdevice.h>
#include <ipxe/ip.h>
#include <usr/route.h>

/** @file
 *
 * IPv4 routing management
 *
 */

/**
 * Print IPv4 routing table
 *
 * @v netdev		Network device
 */
static void route_ipv4_print ( struct net_device *netdev ) {
	struct ipv4_miniroute *miniroute;
	struct ipv4_miniroute *defroute;
	struct in_addr address;
	struct in_addr network;
	struct in_addr netmask;
	struct in_addr gateway;
	int remote;

	/* Print routing table */
	list_for_each_entry ( miniroute, &ipv4_miniroutes, list ) {

		/* Skip non-matching network devices */
		if ( miniroute->netdev != netdev )
			continue;
		address = miniroute->address;
		network = miniroute->network;
		netmask = miniroute->netmask;
		gateway = miniroute->gateway;
		assert ( ( network.s_addr & ~netmask.s_addr ) == 0 );

		/* Defer default routes to be printed with local addresses */
		if ( ! netmask.s_addr )
			continue;

		/* Print local address and destination subnet */
	        remote = ( ( address.s_addr ^ network.s_addr ) &
			   netmask.s_addr );
		printf ( "%s: %s", netdev->name, inet_ntoa ( address ) );
		if ( remote )
			printf ( " for %s", inet_ntoa ( network ) );
		printf ( "/%s", inet_ntoa ( netmask ) );
		if ( gateway.s_addr )
			printf ( " gw %s", inet_ntoa ( gateway ) );

		/* Print default routes with local subnets */
		list_for_each_entry ( defroute, &ipv4_miniroutes, list ) {
			if ( ( defroute->netdev == netdev ) &&
			     ( defroute->address.s_addr = address.s_addr ) &&
			     ( ! defroute->netmask.s_addr ) && ( ! remote ) ) {
				printf ( " gw %s",
					 inet_ntoa ( defroute->gateway ) );
			}
		}

		/* Print trailer */
		if ( ! netdev_is_open ( netdev ) )
			printf ( " (inaccessible)" );
		printf ( "\n" );
	}
}

/** IPv4 routing family */
struct routing_family ipv4_routing_family __routing_family ( ROUTING_IPV4 ) = {
	.print = route_ipv4_print,
};
