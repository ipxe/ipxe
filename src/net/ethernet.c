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
#include <vsprintf.h>
#include <gpxe/if_arp.h>
#include <gpxe/if_ether.h>
#include <gpxe/netdevice.h>
#include <gpxe/pkbuff.h>
#include <gpxe/arp.h>
#include <gpxe/ethernet.h>

/** @file
 *
 * Ethernet protocol
 *
 */

/** Ethernet broadcast MAC address */
static uint8_t eth_broadcast[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/**
 * Perform Ethernet routing
 *
 * @v nethdr	Generic network-layer header
 * @ret llhdr	Generic link-layer header
 * @ret rc	Return status code
 *
 * Constructs the generic link-layer header based on the generic
 * network-layer header, i.e. maps network-layer addresses (e.g. IPv4
 * addresses) to MAC addresses.
 *
 * If the destination MAC address cannot be determined, an ARP request
 * is sent for the requested network-layer address and -ENOENT is
 * returned.
 */
static int eth_route ( struct net_device *netdev,
		       const struct net_header *nethdr,
		       struct ll_header *llhdr ) {
	int rc;

	/* Fill in the easy bits */
	llhdr->net_proto = nethdr->net_protocol->net_proto;
	memcpy ( llhdr->source_ll_addr, netdev->ll_addr, ETH_ALEN );

	/* Work out the destination MAC address */
	if ( nethdr->flags & PKT_FL_BROADCAST ) {
		memcpy ( llhdr->dest_ll_addr, eth_broadcast, ETH_ALEN );
	} else if ( nethdr->flags & PKT_FL_RAW_ADDR ) {
		memcpy ( llhdr->dest_ll_addr, nethdr->dest_net_addr, ETH_ALEN);
	} else if ( nethdr->flags & PKT_FL_MULTICAST ) {
		/* IP multicast is a special case; there exists a
		 * direct mapping from IP address to MAC address
		 */
		assert ( nethdr->net_protocol->net_proto == htons(ETH_P_IP) );
		llhdr->dest_ll_addr[0] = 0x01;
		llhdr->dest_ll_addr[1] = 0x00;
		llhdr->dest_ll_addr[2] = 0x5e;
		llhdr->dest_ll_addr[3] = nethdr->dest_net_addr[1] & 0x7f;
		llhdr->dest_ll_addr[4] = nethdr->dest_net_addr[2];
		llhdr->dest_ll_addr[5] = nethdr->dest_net_addr[3];
	} else {
		/* Otherwise, look up the address using ARP */
		if ( ( rc = arp_resolve ( netdev, nethdr, llhdr ) ) != 0 )
			return rc;
	}

	return 0;
}

/**
 * Fill in Ethernet link-layer header
 *
 * @v pkb	Packet buffer
 * @v llhdr	Generic link-layer header
 *
 * Fills in the Ethernet link-layer header in the packet buffer based
 * on information in the generic link-layer header.
 */
static void eth_fill_llh ( const struct ll_header *llhdr,
			   struct pk_buff *pkb ) {
	struct ethhdr *ethhdr = pkb->data;

	memcpy ( ethhdr->h_dest, llhdr->dest_ll_addr, ETH_ALEN );
	memcpy ( ethhdr->h_source, llhdr->source_ll_addr, ETH_ALEN );
	ethhdr->h_protocol = llhdr->net_proto;
}

/**
 * Parse Ethernet link-layer header
 *
 * @v pkb	Packet buffer
 * @v llhdr	Generic link-layer header
 *
 * Fills in the generic link-layer header based on information in the
 * Ethernet link-layer header in the packet buffer.
 */
static void eth_parse_llh ( const struct pk_buff *pkb,
			    struct ll_header *llhdr ) {
	struct ethhdr *ethhdr = pkb->data;

	memcpy ( llhdr->dest_ll_addr, ethhdr->h_dest, ETH_ALEN );
	memcpy ( llhdr->source_ll_addr, ethhdr->h_source, ETH_ALEN );
	llhdr->net_proto = ethhdr->h_protocol;

	if ( memcmp ( ethhdr->h_dest, eth_broadcast, ETH_ALEN ) == 0 ) {
		llhdr->flags = PKT_FL_BROADCAST;
	} else if ( ethhdr->h_dest[0] & 0x01 ) {
		llhdr->flags = PKT_FL_MULTICAST;
	} else {
		llhdr->flags = 0;
	}
}

/**
 * Transcribe Ethernet address
 *
 * @v ll_addr	Link-layer address
 * @ret string	Link-layer address in human-readable format
 */
static const char * eth_ntoa ( const void *ll_addr ) {
	static char buf[18]; /* "00:00:00:00:00:00" */
	uint8_t *eth_addr = ll_addr;

	sprintf ( buf, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
		  eth_addr[0], eth_addr[1], eth_addr[2],
		  eth_addr[3], eth_addr[4], eth_addr[5] );
	return buf;
}

/** Ethernet protocol */
struct ll_protocol ethernet_protocol = {
	.name = "Ethernet",
	.ll_proto = htons ( ARPHRD_ETHER ),
	.ll_addr_len = ETH_ALEN,
	.ll_header_len = ETH_HLEN,
	.route = eth_route,
	.fill_llh = eth_fill_llh,
	.parse_llh = eth_parse_llh,
	.ntoa = eth_ntoa,
};

LL_PROTOCOL ( ethernet_protocol );
