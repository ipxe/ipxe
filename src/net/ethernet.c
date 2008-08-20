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
#include <stdio.h>
#include <string.h>
#include <byteswap.h>
#include <errno.h>
#include <assert.h>
#include <gpxe/if_arp.h>
#include <gpxe/if_ether.h>
#include <gpxe/netdevice.h>
#include <gpxe/iobuf.h>
#include <gpxe/ethernet.h>

/** @file
 *
 * Ethernet protocol
 *
 */

/** Ethernet broadcast MAC address */
static uint8_t eth_broadcast[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/**
 * Add Ethernet link-layer header
 *
 * @v iobuf		I/O buffer
 * @v netdev		Network device
 * @v net_protocol	Network-layer protocol
 * @v ll_dest		Link-layer destination address
 */
static int eth_push ( struct io_buffer *iobuf, struct net_device *netdev,
		      struct net_protocol *net_protocol,
		      const void *ll_dest ) {
	struct ethhdr *ethhdr = iob_push ( iobuf, sizeof ( *ethhdr ) );

	/* Build Ethernet header */
	memcpy ( ethhdr->h_dest, ll_dest, ETH_ALEN );
	memcpy ( ethhdr->h_source, netdev->ll_addr, ETH_ALEN );
	ethhdr->h_protocol = net_protocol->net_proto;

	return 0;
}

/**
 * Remove Ethernet link-layer header
 *
 * @v iobuf		I/O buffer
 * @v netdev		Network device
 * @v net_proto		Network-layer protocol, in network-byte order
 * @v ll_source		Source link-layer address
 * @ret rc		Return status code
 */
static int eth_pull ( struct io_buffer *iobuf,
		      struct net_device *netdev __unused,
		      uint16_t *net_proto, const void **ll_source ) {
	struct ethhdr *ethhdr = iobuf->data;

	/* Sanity check */
	if ( iob_len ( iobuf ) < sizeof ( *ethhdr ) ) {
		DBG ( "Ethernet packet too short (%zd bytes)\n",
		      iob_len ( iobuf ) );
		return -EINVAL;
	}

	/* Strip off Ethernet header */
	iob_pull ( iobuf, sizeof ( *ethhdr ) );

	/* Fill in required fields */
	*net_proto = ethhdr->h_protocol;
	*ll_source = ethhdr->h_source;

	return 0;
}

/**
 * Transcribe Ethernet address
 *
 * @v ll_addr	Link-layer address
 * @ret string	Link-layer address in human-readable format
 */
const char * eth_ntoa ( const void *ll_addr ) {
	static char buf[18]; /* "00:00:00:00:00:00" */
	const uint8_t *eth_addr = ll_addr;

	sprintf ( buf, "%02x:%02x:%02x:%02x:%02x:%02x",
		  eth_addr[0], eth_addr[1], eth_addr[2],
		  eth_addr[3], eth_addr[4], eth_addr[5] );
	return buf;
}

/** Ethernet protocol */
struct ll_protocol ethernet_protocol __ll_protocol = {
	.name		= "Ethernet",
	.ll_proto	= htons ( ARPHRD_ETHER ),
	.ll_addr_len	= ETH_ALEN,
	.ll_header_len	= ETH_HLEN,
	.ll_broadcast	= eth_broadcast,
	.push		= eth_push,
	.pull		= eth_pull,
	.ntoa		= eth_ntoa,
};
