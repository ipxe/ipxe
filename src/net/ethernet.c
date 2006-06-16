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
#include <gpxe/ethernet.h>

/** @file
 *
 * Ethernet protocol
 *
 */

/** Ethernet broadcast MAC address */
static uint8_t eth_broadcast[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/**
 * Transmit Ethernet packet
 *
 * @v pkb		Packet buffer
 * @v netdev		Network device
 * @v net_protocol	Network-layer protocol
 * @v ll_dest		Link-layer destination address
 *
 * Prepends the Ethernet link-layer header and transmits the packet.
 */
static int eth_transmit ( struct pk_buff *pkb, struct net_device *netdev,
			  struct net_protocol *net_protocol,
			  const void *ll_dest ) {
	struct ethhdr *ethhdr = pkb_push ( pkb, ETH_HLEN );

	memcpy ( ethhdr->h_dest, ll_dest, ETH_ALEN );
	memcpy ( ethhdr->h_source, netdev->ll_addr, ETH_ALEN );
	ethhdr->h_protocol = net_protocol->net_proto;
	return netdev_transmit ( netdev, pkb );
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
	const uint8_t *eth_addr = ll_addr;

	sprintf ( buf, "%02x:%02x:%02x:%02x:%02x:%02x",
		  eth_addr[0], eth_addr[1], eth_addr[2],
		  eth_addr[3], eth_addr[4], eth_addr[5] );
	return buf;
}

/** Ethernet protocol */
struct ll_protocol ethernet_protocol = {
	.name		= "Ethernet",
	.ll_proto	= htons ( ARPHRD_ETHER ),
	.ll_addr_len	= ETH_ALEN,
	.ll_broadcast	= eth_broadcast,
	.transmit	= eth_transmit,
	.parse_llh	= eth_parse_llh,
	.ntoa		= eth_ntoa,
};

LL_PROTOCOL ( ethernet_protocol );
