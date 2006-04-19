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

#include <stdint.h>
#include <string.h>
#include <byteswap.h>
#include <assert.h>
#include <gpxe/if_ether.h>
#include <gpxe/netdevice.h>
#include <gpxe/pkbuff.h>
#include <gpxe/arp.h>

/** @file
 *
 * Ethernet protocol
 *
 */

/** Ethernet broadcast MAC address */
static uint8_t eth_broadcast[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/**
 * Build Ethernet link-layer header
 *
 * @v netdev	Network device
 * @v pkb	Packet buffer
 * @ret rc	Return status code
 *
 * This constructs the Ethernet link-layer header (destination MAC,
 * source MAC, network-layer protocol) based on the metadata found in
 * @c pkb.
 *
 * If the destination MAC address cannot be determined, an ARP request
 * is sent for the requested network-layer address instead.
 */
int eth_build_llh ( struct net_device *netdev, struct pk_buff *pkb ) {
	struct ethhdr *ethhdr = pkb->data;
	const void *eth_dest;
	int rc;

	/* Do the easy bits */
	ethhdr->h_protocol = pkb->net_proto;
	memcpy ( ethhdr->h_source, netdev->ll_addr,
		 sizeof ( ethhdr->h_source ) );

	/* Work out the destination MAC address */
	if ( pkb->flags & PKB_FL_RAW_NET_ADDR ) {
		eth_dest = pkb->net_addr;
	} else if ( pkb->flags & PKB_FL_BROADCAST ) {
		eth_dest = eth_broadcast;
	} else if ( pkb->flags & PKB_FL_MULTICAST ) {
		/* IP multicast is a special case; there exists a
		 * direct mapping from IP address to MAC address
		 */
		assert ( pkb->net_proto == htons ( ETH_P_IP ) );
		ethhdr->h_dest[0] = 0x01;
		ethhdr->h_dest[1] = 0x00;
		ethhdr->h_dest[2] = 0x5e;
		ethhdr->h_dest[3] = *( ( char * ) pkb->net_addr + 1 ) & 0x7f;
		ethhdr->h_dest[4] = *( ( char * ) pkb->net_addr + 2 );
		ethhdr->h_dest[5] = *( ( char * ) pkb->net_addr + 3 );
		eth_dest = ethhdr->h_dest;
	} else {
		/* Otherwise, look up the address using ARP */
		if ( ( rc = arp_resolve ( netdev, pkb, &eth_dest ) ) != 0 )
			return rc;
	}

	/* Fill in destination MAC address */
	memcpy ( ethhdr->h_dest, eth_dest, sizeof ( ethhdr->h_dest ) );

	return 0;
}

/**
 * Parse Ethernet link-layer header
 *
 * @v netdev	Network device
 * @v pkb	Packet buffer
 * @ret rc	Return status code
 *
 * This parses the Ethernet link-layer header (destination MAC, source
 * MAC, network-layer protocol) and fills in the metadata in @c pkb.
 */
int eth_parse_llh ( struct net_device *netdev __unused, struct pk_buff *pkb ) {
	struct ethhdr *ethhdr = pkb->data;

	pkb->net_proto = ethhdr->h_protocol;
	pkb->flags = PKB_FL_RAW_NET_ADDR;
	pkb->net_addr_len = sizeof ( ethhdr->h_dest );
	pkb->net_addr = ethhdr->h_dest;

	if ( memcmp ( ethhdr->h_dest, eth_broadcast,
		      sizeof ( ethhdr->h_dest ) ) == 0 ) {
		pkb->flags |= PKB_FL_BROADCAST;
	} else if ( ethhdr->h_dest[0] & 0x01 ) {
		pkb->flags |= PKB_FL_MULTICAST;
	}

	return 0;
}
